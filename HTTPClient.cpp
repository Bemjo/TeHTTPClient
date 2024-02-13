#include "HTTPClient.h"



HTTPClient::HTTPClient(Client &client, unsigned long timeout) :
  client(&client),
  currentParsingConnection(std::make_shared<ConnectionInformation>())
{
  client.setTimeout(timeout);
}



HTTPClient::~HTTPClient()
{
  close();
}



void HTTPClient::close()
{
  if (client->connected())
  {
    client->stop();
  }
}



/**
 * @brief Sends an HTML request string to a given hostname.
 * NOTE: This opens a connection to the given host, and is cleaned up only on errors. You must handle closing the client after handling the body.
 *
 * @param hostname The hostname to lookup
 * @param port The port to connect to
 * @param request The HTML Request line to send
 * @param headers The HTML headers to send
 * @param timeout The timeout in milliseconds to wait for a response
 * @param outHeaders If not null, the parsed header lines will be pushed onto the back
 *
 * @return -1 on failure to connect to hostname
 * @return -2 on failure to send request to hostname
 * @return HTML Status code
 */
std::shared_ptr<ConnectionInformation> HTTPClient::sendHTMLRequest(
      const char* hostname,
      uint16_t port,
      const char* request,
      const char* inHeaders,
      std::vector<String>* outHeaders) {
  Serial.printf(F("[HTTPClient]: Attemping to connect to %s:%hu\n"), hostname, port);

  if (!client->connect(hostname, port)) {
    Serial.printf(F("[HTTPClient]: Connection to %s:%hu failed\n"), hostname, port);
    return nullptr;
  }

  Serial.printf(F("[HTTPClient]: Connected to %s:%hu\n"), hostname, port);
  Serial.printf(F("[HTTPClient]: Sending Request\n    %s\n"), request);

  // Send our request to the server, and set required headers
  client->println(request);
  client->printf(F("Host: %s:%hu\r\n"), hostname, port);

  // Send any valid headers passed in
  if (inHeaders != nullptr && inHeaders[0] != '\0') {
    client->println(*inHeaders);
  }

  // Finalize the request and ensure it was received
  if (client->println() == 0) {
    Serial.printf(F("[HTTPClient]: Failed to send request to %s:%hu\n"), hostname, port);

    close();
    return nullptr;
  }

  delay(2); // Wait a moment such that the client has time to process our request

  // Return the http response code from the server
  return readResponseStatus(outHeaders);
}



// Reads the status line from the HTTP response, optionally returning the parsed headers if there is a vector allocated for it
std::shared_ptr<ConnectionInformation> HTTPClient::readResponseStatus(std::vector<String>* outHeaders) {
  String status;

  // Ignore all empty lines before the response line, damn webservers not adhering to the standard!
  while (client->connected() && (status = client->readStringUntil('\r').trim()).length() == 0);

  client->read();    // discard \n

  Serial.printf(F("[HTTPClient] Recieved response status: %s\n"), status.c_str());

  sscanf(status.c_str(), "%*s %hu %*s", &currentParsingConnection->return_status);

  return readHeaders(currentParsingConnection, outHeaders);
}



// param outHeaders - Optionally 
// returns ConnectionInformation& a reference to the current connection state
std::shared_ptr<ConnectionInformation>& HTTPClient::readHeaders(std::shared_ptr<ConnectionInformation>& connection, std::vector<String>* outHeaders) {
  Serial.println("[HTTPClient] Parsing headers...");
  connection->encoding = HTTPTransferEncoding::None;

  // Read size
  const int BUF_SIZE = HEADER_READ_BUFFER_SIZE;

  String header, lower;
  
  while ( (header = client->readStringUntil('\r', BUF_SIZE)).length() > 0) {
    client->read(); // Discard \n

    if (outHeaders != nullptr) {
      outHeaders->push_back(header);
    }

    Serial.printf(F("Header --- %s\n"), header.c_str());

    lower = header;
    lower.toLowerCase();

    if (connection->encoding == HTTPTransferEncoding::None) {
      if (lower.startsWith(F("transfer-encoding"))) {
        Serial.println(F("[HTTPClient] message has special encoding"));
        if (lower.endsWith(F("chunked"))) { connection->encoding = HTTPTransferEncoding::Chunked; }
        else if (lower.endsWith(F("compress"))) { connection->encoding = HTTPTransferEncoding::Compress; }
        else if (lower.endsWith(F("deflate"))) { connection->encoding = HTTPTransferEncoding::Deflate; }
        else if (lower.endsWith(F("gzip"))) { connection->encoding = HTTPTransferEncoding::GZip; }
      } else if (lower.startsWith(F("content-length"))) {
        int k = lower.indexOf(' ');
        connection->chunkSize = strtoul((const char*)(header.c_str() + k + 1), nullptr, 10);
        Serial.printf(F("[HTTPClient] No chunked encoding, content length is %lu bytes\n"), connection->chunkSize);
      }
    }
  }

  Serial.println(F("[HTTPClient] Finished Parsing headers"));

  return connection;
}



/// <summary>
/// Streams the entire HTTP response body into the provided buffer in bufferSize chunks, calling the writeCallback function anytime at least 1 byte was read
/// </summary>
/// <param name="buffer">User provided buffer to stream data into</param>
/// <param name="bufferSize">the size of the buffer</param>
/// <param name="writeCallback">Callback invoked when bytes are read into the buffer</param>
/// <returns>The total number of bytes processed</returns>
long int HTTPClient::readBody(uint8_t* buffer, size_t bufferSize, std::function<bool(uint8_t *buffer, size_t dataSize)> writeCallback) {
  static size_t r;
  size_t total = 0;

  // continue reading while we're expecting more data
  do {
    r = readBytes((char*)buffer, bufferSize);
    total += r;

    Serial.printf(F("[HTTPClient] successfully read %ld bytes\n"), r);

    if (!writeCallback(buffer, r)) {
      Serial.println(F("[HTTPClient] Write callback failed"));
      return -1;
    }
  } while (r == bufferSize);

  return total;
}



long int HTTPClient::readBody(uint8_t* buffer, size_t bufferSize, HTTP_WRITE_CALLBACK writeCallback) {
  return readBody(buffer, bufferSize, std::bind(writeCallback, std::placeholders::_1, std::placeholders::_2));
}



long int HTTPClient::readBody(String& body, size_t maxCharacters) {
  static size_t n;

  n = body.length();
  body = client->readString(maxCharacters);

  return body.length() - n;
}



// Read a single byte from the http stream
int HTTPClient::read() {
  static int a;

  if (currentParsingConnection->chunkSize == 0 && currentParsingConnection->encoding == EHTTPTransferEncoding::Chunked) {
    currentParsingConnection->chunkSize = readChunkedDataSize();

    // We're at the end of our data, finish off reading it, and return -1 as required
    if (currentParsingConnection->chunkSize == 0) {
      Serial.println(F("[HTTPClient] Read: EOF"));
      client->read(); // discard \r
      client->read(); // discard \n

      return -1;
    }
  }

  if ( (a = client->read()) > 0) {
    --currentParsingConnection->chunkSize;
  }

  return a;
}



/// <summary>
/// Reads bytes from the http body stream for the currently open client
/// </summary>
/// <param name="buffer">A pointer to a buffer that is at least length bytes long</param>
/// <param name="length">The number of bytes to read from the HTTP stream</param>
/// <returns>The number of bytes read into buffer</returns>
size_t HTTPClient::readBytes(char* buffer, size_t length) {
  // Quick shortcircuit for reuqests smaller than current chunk size
  if (length <= currentParsingConnection->chunkSize) {
    currentParsingConnection->chunkSize -= length;
    return client->readBytes(buffer, length);
  }

  size_t len = length;  // How much we have left we want to read into the buffer
  size_t readSize, r;

  // We're going to hit a chunk size boundary at least once for the given request that we'll need to parse
  while (len > 0) {
    // Read the rest of len, or read upto the chunk boundary
    readSize = (len < currentParsingConnection->chunkSize) ? len : currentParsingConnection->chunkSize;

    r = client->readBytes(buffer + (length - len), readSize);

    len -= r;
    currentParsingConnection->chunkSize -= r;

    // We're at a chunk boundary, parse the stupid thing
    if (currentParsingConnection->chunkSize == 0) {
      if (currentParsingConnection->encoding == EHTTPTransferEncoding::Chunked) {
        // A Boundary is formatted like:
        // DATA1...\r\n
        // CHUNKSIZE\r\n
        // DATA2...\r\n
        // so skip the \r\n after DATA_n
        client->read();
        client->read();

        currentParsingConnection->chunkSize = readChunkedDataSize();

        // The next chunk has a size of 0, this is the end of the body data
        if (currentParsingConnection->chunkSize == 0) {
          client->read();
          client->read();

          break;
        }
      }
      else {  // No special encoding, and we're done reading
        break;
      }
    }
  }

  // do a quick calc instead of using another var
  return length - len;
}



size_t HTTPClient::readChunkedDataSize() {
  static char csBuf[9];
  // Ensure we're at the start of our line
  do {
    csBuf[0] = client->read();
  } while (csBuf[0] == '\n' || csBuf[0] == '\r');

  // read in the chunk size, a hex formatted number
  client->readBytesUntil('\r', csBuf+1, 8);
  client->read(); // discard \n

  return strtoul(csBuf, nullptr, 16);
}



bool HTTPClient::readBody(DynamicJsonDocument& outDoc) {
  if (currentParsingConnection->encoding == HTTPTransferEncoding::Chunked) {
    currentParsingConnection->chunkSize = readChunkedDataSize();
  }

  DeserializationError err;
  ReadBufferingClient rbc(*this, 128);

  err = deserializeJson(outDoc, rbc);

  if (err) {
    Serial.print(F("[HTTPClient] There was an error parsing the JSON response: "));
    Serial.println(err.f_str());
  }

  return !err;
}

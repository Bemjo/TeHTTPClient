#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H



#include <Arduino.h>
#include <Client.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#include <stdint.h>
#include <vector>
#include <functional>
#include <memory>



#define HTTP_VER_STR        F(" HTTP/1.1")
#define MAKE_HTTP_CH(x, y)  (x y HTTP_VER_STR)
#define MAKE_HTTP(x, y)     (x + y + HTTP_VER_STR).c_str()

#define MAKE_GET(x)         MAKE_HTTP(F("GET "), x)
#define MAKE_PUT(x)         MAKE_HTTP(F("PUT "), x)
#define MAKE_POST(x)        MAKE_HTTP(F("POST "), x)
#define MAKE_DELETE(x)      MAKE_HTTP(F("DELETE "), x)
#define MAKE_HEAD(x)        MAKE_HTTP(F("HEAD "), x)
#define MAKE_PATCH(x)       MAKE_HTTP(F("PATCH "), x)

#define MAKE_GET_CH(x)         MAKE_HTTP_CH(F("GET "), x)
#define MAKE_PUT_CH(x)         MAKE_HTTP_CH(F("PUT "), x)
#define MAKE_POST_CH(x)        MAKE_HTTP_CH(F("POST "), x)
#define MAKE_DELETE_CH(x)      MAKE_HTTP_CH(F("DELETE "), x)
#define MAKE_HEAD_CH(x)        MAKE_HTTP_CH(F("HEAD "), x)
#define MAKE_PATCH_CH(x)       MAKE_HTTP_CH(F("PATCH "), x)

#define HEADER_READ_BUFFER_SIZE 2048



typedef enum EHTTPTransferEncoding : uint8_t {
  None,
  Chunked,
  Compress,
  Deflate,
  GZip,
} HTTPTransferEncoding;



struct ConnectionInformation {
  size_t chunkSize = 0;
  uint16_t return_status = 0;
  HTTPTransferEncoding encoding = HTTPTransferEncoding::None; 
};



typedef bool(*HTTP_WRITE_CALLBACK)(uint8_t* buffer, size_t bufferSize);



class HTTPClient : public Client
{
public:
  HTTPClient(Client &client, unsigned long timeout = 5000);
  virtual ~HTTPClient();

  std::shared_ptr<ConnectionInformation> http_put(const char* hostname, uint16_t port, const String& request, const char* inHeaders, std::vector<String> *outHeaders)
    { return sendHTMLRequest(hostname, port, MAKE_PUT(request), inHeaders, outHeaders); }
  std::shared_ptr<ConnectionInformation> http_get(const char* hostname, uint16_t port, const String& request, const char* inHeaders, std::vector<String> *outHeaders)
    { return sendHTMLRequest(hostname, port, MAKE_GET(request), inHeaders, outHeaders); }
  std::shared_ptr<ConnectionInformation> http_post(const char* hostname, uint16_t port, const String& request, const char* inHeaders, std::vector<String> *outHeaders)
    { return sendHTMLRequest(hostname, port, MAKE_POST(request), inHeaders, outHeaders); }
  std::shared_ptr<ConnectionInformation> http_head(const char* hostname, uint16_t port, const String& request, const char* inHeaders, std::vector<String> *outHeaders)
    { return sendHTMLRequest(hostname, port, MAKE_HEAD(request), inHeaders, outHeaders); }
  std::shared_ptr<ConnectionInformation> http_delete(const char* hostname, uint16_t port, const String& request, const char* inHeaders, std::vector<String> *outHeaders)
    { return sendHTMLRequest(hostname, port, MAKE_DELETE(request), inHeaders, outHeaders); }
  std::shared_ptr<ConnectionInformation> http_patch(const char* hostname, uint16_t port, const String& request, const char* inHeaders, std::vector<String> *outHeaders)
    { return sendHTMLRequest(hostname, port, MAKE_PATCH(request), inHeaders, outHeaders); }

  long int readBody(uint8_t* buffer, size_t bufferSize, std::function<bool(uint8_t *buffer, size_t dataSize)> writeCallback);
  long int readBody(uint8_t* buffer, size_t bufferSize, HTTP_WRITE_CALLBACK writeCallback);
  long int readBody(String& body, size_t maxCharacters);

  bool readBody(DynamicJsonDocument& outDoc);

  template<size_t A>
  bool readBody(DynamicJsonDocument& outDoc, const StaticJsonDocument<A>* filter = nullptr);

  template<size_t A>
  bool readBody(StaticJsonDocument<A>& outDoc);

  template<size_t A, size_t B>
  bool readBody(StaticJsonDocument<A>& outDoc, const StaticJsonDocument<B>* filter = nullptr);

  void setTimeout(unsigned long timeout) {if(client != nullptr) client->setTimeout(timeout);}

  // helper functions for parsing JSON with chunked encoding
  virtual int read() override;
  virtual int read(uint8_t *buf, size_t size) override {return readBytes((char*)buf, size);}
  virtual int available() override
    {return client->available();}
  virtual int peek() override
    {return client->peek();}
  virtual size_t write(uint8_t b) override
    {return client->write(b);}
  virtual int availableForWrite(void)	override
    { return client->availableForWrite(); }
  virtual void flush() override 
    { client->flush(); }
  virtual int connect(IPAddress ip, uint16_t port) override
    {return client->connect(ip, port);}
  virtual int connect(const char *host, uint16_t port) override
    {return client->connect(host, port);}
  virtual size_t write(const uint8_t *buf, size_t size) override
    {return client->write(buf, size);}
  virtual void stop() override
    {client->stop();}
  virtual uint8_t connected() override
    {return client->connected();}
  virtual operator bool() override
    {return (*client);}

  // Shadow the orignal functions
  size_t readBytes(char* buffer, size_t length);

protected:
  std::shared_ptr<ConnectionInformation> sendHTMLRequest(const char* hostname, uint16_t port, const char* request, const char* inHeaders, std::vector<String> *outHeaders);
  std::shared_ptr<ConnectionInformation> readResponseStatus(std::vector<String>* headers);
  std::shared_ptr<ConnectionInformation>& readHeaders(std::shared_ptr<ConnectionInformation>& connection, std::vector<String>* headers);
  size_t readChunkedDataSize();
  void close();

protected:
  Client *client;
  std::shared_ptr<ConnectionInformation> currentParsingConnection;
};



template<size_t A>
bool HTTPClient::readBody(DynamicJsonDocument& outDoc, const StaticJsonDocument<A>* filter)
{
  if (currentParsingConnection->encoding == HTTPTransferEncoding::Chunked)
  {
    currentParsingConnection->chunkSize = readChunkedDataSize();
  }

  DeserializationError err;
  ReadBufferingClient rbc(*this, 128);

  if (filter != nullptr)
  {
    err = deserializeJson(outDoc, rbc, DeserializationOption::Filter(*filter));
  }
  else
  {
    err = deserializeJson(outDoc, rbc);
  }

  if (err)
  {
    Serial.print(F("[HTTPClient] There was an error parsing the JSON response: "));
    Serial.println(err.f_str());
  }

  return err;
}



template<size_t A>
bool HTTPClient::readBody(StaticJsonDocument<A>& outDoc)
{
  if (currentParsingConnection->encoding == HTTPTransferEncoding::Chunked)
  {
    currentParsingConnection->chunkSize = readChunkedDataSize();
  }

  DeserializationError err;
  ReadBufferingClient rbc(*this, 128);

  err = deserializeJson(outDoc, rbc);

  if (err)
  {
    Serial.print(F("[HTTPClient] There was an error parsing the JSON response: "));
    Serial.println(err.f_str());
  }

  return err;
}



template<size_t A, size_t B>
bool HTTPClient::readBody(StaticJsonDocument<A>& outDoc, const StaticJsonDocument<B>* filter)
{
  if (currentParsingConnection->encoding == HTTPTransferEncoding::Chunked)
  {
    currentParsingConnection->chunkSize = readChunkedDataSize();
  }

  DeserializationError err;
  ReadBufferingClient rbc(*this, 128);

  if (filter != nullptr)
  {
    err = deserializeJson(outDoc, rbc, DeserializationOption::Filter(*filter));
  }
  else
  {
    err = deserializeJson(outDoc, rbc);
  }

  if (err)
  {
    Serial.print(F("[HTTPClient] There was an error parsing the JSON response: "));
    Serial.println(err.f_str());
  }

  return err;
}



#endif // HTTP_CLIENT_H

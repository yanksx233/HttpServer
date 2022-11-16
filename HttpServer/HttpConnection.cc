#include "HttpConnection.h"
#include "base/TcpConnection.h"
#include "base/Buffer.h"
#include "base/Timestamp.h"
#include "base/Logging.h"
#include "base/EventLoop.h"

#include <regex>
#include <sys/mman.h>
#include <fcntl.h>
#include <boost/any.hpp>


void timeoutCallback(std::weak_ptr<TcpConnection> tiedConn) {
  TcpConnectionPtr conn = tiedConn.lock();
  if (conn) {
    LOG_DEBUG << "timeout achieved, close connection";
    conn->shutdown();
  }
}

void onConnection(const TcpConnectionPtr& conn) {
  if (conn->connected()) {
    HttpConnectionPtr httpData(new HttpConnection("./resources"));
    httpData->setTimerId(conn->getLoop()->runAfter(std::bind(timeoutCallback, conn), 60));
    conn->setContext(httpData);
  }
  else {
    HttpConnectionPtr httpData = boost::any_cast<HttpConnectionPtr>(conn->getContext());
    conn->getLoop()->cancel(httpData->getTimerId());
    LOG_DEBUG << conn->name() << " is down";
  }
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp t) {
  // LOG_DEBUG << "read buf:\n" << std::string(buf->beginRead(), buf->readableBytes());

  HttpConnectionPtr httpData = boost::any_cast<HttpConnectionPtr>(conn->getContext());
  conn->getLoop()->cancel(httpData->getTimerId());
  httpData->setTimerId(conn->getLoop()->runAfter(std::bind(timeoutCallback, conn), 60));
  httpData->processMessage(conn, buf, t);
}


const std::map<int, std::string> HttpConnection::kResponses = {
  {200, "OK"},
  {400, "Bad Request"},
  {403, "Forbidden"},
  {404, "Not Found"},
};

const std::map<std::string, std::string> HttpConnection::kMimeType = {
  {".html",   "text/html"},
  {".xml",    "text/xml"},
  {".xhtml",  "application/xhtml+xml"},
  {".txt",    "text/plain"},
  {".rtf",    "application/rtf"},
  {".pdf",    "application/pdf"},
  {".doc",    "application/msword"},
  {".png",    "image/png"},
  {".gif",    "image/gif"},
  {".jpg",    "image/jpeg"},
  {".jpeg",   "image/jpeg"},
  {".au",     "audio/basic"},
  {".mpeg",   "video/mpeg"},
  {".mpg",    "video/mpeg"},
  {".avi",    "video/x-msvideo"},
  {".gz",     "application/x-gzip"},
  {".tar",    "application/x-tar"},
  {".css",    "text/css"},
  {".js",     "text/javascript"},
};

// const std::map<std::string, bool> HttpConnection::kPostUserVerify = {
//   {"/register.html", false},
//   {"/login.html",    true},
// };

HttpConnection::HttpConnection(const std::string& sourceDir)
  : parseState_(kRequestLine),
    responseCode_(-1),
    keepAlive_(false),
    kSourceDir(sourceDir)
  {}

void HttpConnection::processMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
  HttpCode parseRet = parseRequest(buf);
  if (parseRet == kNoRequest) {
    return;
  }

  makeResponse(conn->outputBuffer(), parseRet);
  conn->send(conn->outputBuffer());
  if (!keepAlive_) {
    conn->shutdown();
  }
  else {
    resetState();
  }
}

HttpConnection::HttpCode HttpConnection::parseRequest(Buffer* inputBuf) {
  while (parseState_ != kFinish) {
    const char* crlf = inputBuf->findCrlf();  // pos or nullptr
    std::string line;
    if (parseState_ == kBody) {  // FIXME: 由 Content-Length 读取 body，认为 body 没有结尾标识符 CRLF
      assert(header_.find("Content-Length") != header_.end());
      size_t remain = std::stoul(header_["Content-Length"]) - body_.size();
      if (inputBuf->readableBytes() < remain) {
        return kNoRequest;
      }
      line += inputBuf->retrieveAsString(remain);
    }
    else if (crlf == nullptr) {  // FIXME: 没考虑\r和\n单独出现的情况
      LOG_DEBUG << "HttpConnection::parse(): no CRLF in message";
      return kNoRequest;
    }
    else {
      line += inputBuf->retrieveAsString(static_cast<size_t>(crlf - inputBuf->beginRead()));
      inputBuf->retrieveUntil(crlf+2);
    }

    HttpCode ret = kNoRequest;
    switch (parseState_) {
      case kRequestLine:
        ret = parseRequestLine(line);
        if (ret != kNoRequest) {
          return ret;
        }
        break;

      case kHeader:
        ret = parseRequestHeader(line);
        if (ret != kNoRequest) {
          return ret;
        }
        break;

      case kBody:
        ret = parseRequestBody(line);
        if (ret != kNoRequest) {
          return ret;
        }
        break;

      default:
        break;
    }  // switch
  }  // while

  assert(parseState_ == kFinish);  // 似乎unreachable
  return kGetRequest;
}

HttpConnection::HttpCode HttpConnection::parseRequestLine(const std::string& line) {
  std::regex pattern("^([^ ]+) ([^ ]+) HTTP/([^ ]+)$");
  std::smatch result;
  if (!std::regex_match(line, result, pattern)) {
    LOG_DEBUG << "HttpConnection::parseRequestLine(): bad request line";
    return kBadRequest;
  }
  method_ = result[1];
  path_ = result[2];
  version_ = result[3];
  parseState_ = kHeader;

  if (path_ == "/") {
    path_ += "index.html";
  }
  else if (path_.find('.') == std::string::npos) {
    path_ += ".html";
  }

  if (::stat((kSourceDir+path_).c_str(), &requestFileStat_) < 0 
      || S_ISDIR(requestFileStat_.st_mode)) {
    LOG_DEBUG << "HttpConnection::parseRequestLine(): no resource";
    return kNoResource;
  }

  if (!(requestFileStat_.st_mode & S_IROTH)) {
    LOG_DEBUG << "HttpConnection::parseRequestLine(): no permission";
    return kForbidden;
  }

  return kNoRequest;
}

HttpConnection::HttpCode HttpConnection::parseRequestHeader(const std::string& line) {
  if (line.size() == 0) { // header末尾的CRLF
    if (header_.find("Content-Length") == header_.end()
        || (stoi(header_.at("Content-Length"))) == 0) {
      parseState_ = kFinish;
      return kGetRequest;
    }
    else {
      parseState_ = kBody;
      return kNoRequest;
    }
  }

  std::regex pattern("^([^:]+): *(.+)$");
  std::smatch result;
  if (!std::regex_match(line, result, pattern)) {
    LOG_DEBUG << "HttpConnection::parseHeader(): bad header";
    return kBadRequest;
  }
  header_[result[1]] = result[2];
  return kNoRequest;
}

HttpConnection::HttpCode HttpConnection::parseRequestBody(const std::string& line) {
  body_ += line;
  size_t contentLength = stoul(header_.at("Content-Length"));
  if (body_.size() < contentLength) {
    return kNoRequest;
  }
  else if (body_.size() > contentLength) {
    LOG_DEBUG << "HttpConnection::parseBody(): body too long";
    return kBadRequest;
  }

  if (method_ == "POST") {
    HttpCode ret = parsePost();
    if (ret != kNoRequest) {
      return ret;
    }
    ret = userVerify();
    if (ret != kNoRequest) {
      return ret;
    }
  }

  parseState_ = kFinish;
  return kGetRequest;
}

HttpConnection::HttpCode HttpConnection::parsePost() {
  if (body_.size() == 0) {
    LOG_DEBUG << "HttpConnection::parsePost(): empty body";
    return kNoRequest;
  }

  if (header_.find("Content-Type") == header_.end()) {
    LOG_DEBUG << "HttpConnection::parsePost(): unknown Content-Type";
    return kBadRequest;
  }

  if (header_["Content-Type"] == "application/x-www-form-urlencoded") {
    return parseFromUrlEncode();
  }
  // else (other encode type)

  return kNoRequest;
}

HttpConnection::HttpCode HttpConnection::parseFromUrlEncode() {
  std::string key, value;
  std::string* current = &key;
  size_t n = body_.size();
  for (size_t i = 0; i < n; ++i) {
    switch (body_[i]) {
      case '=':
        current = &value;
        break;

      case '&':
        if (key.size() == 0 || value.size() == 0) {
          LOG_DEBUG << "HttpConnection::parseFromUrlEncode(): empty key or value";
          return kBadRequest;
        }
        post_[key] = value;
        key.clear();
        value.clear();
        current = &key;
        break;

      case '+':
        *current += ' ';
        break;

      case '%':
        if (i+2 >= n) {
          LOG_DEBUG << "HttpConnection::parseFromUrlEncode(): wrong hex encode";
          return kBadRequest;
        }
        *current += std::to_string(static_cast<int>(
          strtol(body_.substr(i+1, 2).c_str(), nullptr, 16)
          ));
        i += 2;
        break;

      default:
        *current += body_[i];  
        break;
    }
  }

  if (key.size() == 0 || value.size() == 0) {
    LOG_DEBUG << "HttpConnection::parseFromUrlEncode(): empty key or value";
    return kBadRequest;
  }
  post_[key] = value;

  std::string log = "get POST [";
  for (const auto& item : post_) {
    log += item.first + ": " + item.second + ", ";
  }
  LOG_INFO << log << "]";
  return kNoRequest;
}

HttpConnection::HttpCode HttpConnection::userVerify() {
  // TODO: 还未实现
  if (path_.find("register.html") != std::string::npos) {
    path_ = "/welcome.html";
  }
  else if (path_.find("login.html") != std::string::npos) {
    path_ = "/welcome.html";
  }
  else {
    path_ = "/error.html";
  }

  if (::stat((kSourceDir+path_).c_str(), &requestFileStat_) < 0) {
    LOG_DEBUG << "HttpConnection::userVerify(): file no exist";
    return kNoResource;
  }
  return kNoRequest;
}

void HttpConnection::makeResponse(Buffer* outputBuf, HttpCode parseRet) {
  assert(parseRet != kNoRequest);
  initResponse(parseRet);
  makeResponseLine(outputBuf);
  makeResponseHeader(outputBuf);
  makeResponseBody(outputBuf);
}

void HttpConnection::initResponse(HttpCode httpCode) {
  switch (httpCode) {
    case kGetRequest:
      responseCode_ = 200;
      break;
    case kBadRequest:
      responseCode_ = 400;
      break;
    case kForbidden:
      responseCode_ = 403;
      break;
    case kNoResource:
      responseCode_ = 404;
      break;
    default:
      responseCode_ = 400;
      break;
  }

  // 需要前面代码保证path_对应的文件存在，对存在的文件如果下面系统调用都崩溃那404也发不出来，故abort
  if (responseCode_ != 200) {
    path_ = "/" + std::to_string(responseCode_) + ".html";   //  "/40x.html"
    if (::stat((kSourceDir+path_).c_str(), &requestFileStat_) < 0) {
      LOG_SYSFATAL << "HttpConnection::initResponse(), stat error";
    }
  }
}

void HttpConnection::makeResponseLine(Buffer* outputBuf) {
  assert(responseCode_ != -1);
  assert(kResponses.find(responseCode_) != kResponses.end());
  outputBuf->append("HTTP/1.1 " + 
                    std::to_string(responseCode_) +
                    " " + kResponses.at(responseCode_) + "\r\n");
}

void HttpConnection::makeResponseHeader(Buffer* outputBuf) {
  // 框架accept后对connfd设置的keep-alive是TCP选项，这里是HTTP选项
  outputBuf->append("Connection: ");
  if (header_.find("Connection") != header_.end()
      && header_["Connection"] == "keep-alive"
      && version_ == "1.1") {
    keepAlive_ = true;
    outputBuf->append("keep-alive\r\n");
    outputBuf->append("Keep-Alive: max=6, timeout=120\r\n");
  }
  else {
    keepAlive_ = false;
    outputBuf->append("close\r\n");
  }

  outputBuf->append("Content-Type: ");
  size_t pos = path_.find_last_of('.');
  if (pos == std::string::npos) {
    outputBuf->append("text/plain\r\n");
  }
  else {
    std::string suffix = path_.substr(pos);
    if (kMimeType.find(suffix) == kMimeType.end()) {
      outputBuf->append("text/plain\r\n");
    }
    else {
      outputBuf->append(kMimeType.at(suffix) + "\r\n");
    }
  }

  outputBuf->append("Content-Length: " + std::to_string(requestFileStat_.st_size) + "\r\n");

  outputBuf->append("\r\n");
}

void HttpConnection::makeResponseBody(Buffer* outputBuf) {
  int fd = ::open((kSourceDir+path_).c_str(), O_RDONLY);
  if (fd < 0) {
    LOG_SYSFATAL << "HttpConnection::makeResponseBody(), open error";
  }

  void* mmapRet = ::mmap(nullptr, requestFileStat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mmapRet == MAP_FAILED) {
    LOG_SYSFATAL << "HttpConnection::makeResponseBody(), mmap error";
  }
  if (::close(fd) < 0) {
    LOG_SYSERR << "HttpConnection::makeResponseBody(), close error";
  }

  outputBuf->append(static_cast<const char*>(mmapRet), requestFileStat_.st_size);  // 是否会阻塞？  框架会先copy到mmapAddr，再从mmapAddr copy到buffer，再写入sockfd
                                                          // 能否如writev一样直接从mmapAddr写入sockfd，或直接从物理内存移到buffer中？
  if (::munmap(mmapRet, requestFileStat_.st_size) < 0) {
    LOG_SYSERR << "HttpConnection::makeResponseBody(), munmap error";
  }
}

void HttpConnection::resetState() {
  parseState_ = kRequestLine;

  method_.clear();  // TODO: 是否有必要清空？
  path_.clear();
  version_.clear();
  header_.clear();
  body_.clear();

  responseCode_ = -1;
  memset(&requestFileStat_, 0, sizeof(requestFileStat_));
}
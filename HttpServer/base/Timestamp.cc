#include "Timestamp.h"

#include <sys/time.h>


std::string Timestamp::toLocalTime(bool microSecond) {
  if (formatedTime_.empty()) {
    time_t second = static_cast<time_t>(microSecondsSinceEpoch_ / kMircoSecondsPerSecond);
    time_t mircoSecond = static_cast<time_t>(microSecondsSinceEpoch_ % kMircoSecondsPerSecond);

    struct tm* tm_time = localtime(&second);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d,%06ld", tm_time->tm_year+1900, tm_time->tm_mon+1, tm_time->tm_mday,
              tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec, mircoSecond);
    formatedTime_ = buf;
  }
  if (!microSecond) {
    return formatedTime_.substr(0, formatedTime_.find_last_of(','));
  }
  return formatedTime_;
}


Timestamp Timestamp::now() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return Timestamp(tv.tv_sec * kMircoSecondsPerSecond + tv.tv_usec);
}


#ifndef REACTOR_BASE_TIMESTAMP_H
#define REACTOR_BASE_TIMESTAMP_H


#include <string>

class Timestamp /* copyable */ {
 public:
  explicit Timestamp(int64_t microSecondsSinceEpoch = 0)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

  std::string toLocalTime(bool microSecond = true);
  
  static Timestamp now();

  int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
  bool valid() const { return microSecondsSinceEpoch_ > 0; }

  static const int64_t kMircoSecondsPerSecond = 1000 * 1000;

 private:
  int64_t microSecondsSinceEpoch_;
  std::string formatedTime_;
};

inline bool operator<(const Timestamp lhs, const Timestamp rhs) {
  return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(const Timestamp lhs, const Timestamp rhs) {
  return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline Timestamp addTime(Timestamp timestamp, double seconds) {
  int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMircoSecondsPerSecond);
  return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

#endif  // REACTOR_BASE_TIMESTAMP_H
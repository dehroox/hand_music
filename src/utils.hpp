#ifndef UTILS_HPP
#define UTILS_HPP

namespace Utils {
auto continually_retry_ioctl(int file_descriptor, unsigned long request,
                             void* argument) -> int;
}  // namespace Utils

#endif  // UTILS_HPP

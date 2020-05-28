#include <transfertest.hpp>

ACTION transfertest::hi(name user) {
  require_auth(user);
  print("Hello, ", name{user});
}

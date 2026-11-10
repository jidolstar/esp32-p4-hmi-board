#include <cassert>
#include <cstdio>
#include <cstring>

#include "pages/wifi_ap_web/wifi_ap_web_qr.hpp"

int main() {
  using namespace hmi::samples::suite::wifi_ap_web;
  char payload[kMaximumQrPayloadBytes + 1U]{};

  assert(make_wifi_qr_payload("HMI;Control:AB", "pw,word\\1234", payload,
                              sizeof(payload)));
  assert(std::strcmp(payload,
                     "WIFI:T:WPA;S:HMI\\;Control\\:AB;P:pw\\,word\\\\1234;;") ==
         0);
  assert(!make_wifi_qr_payload("", "password", payload, sizeof(payload)));
  assert(!make_wifi_qr_payload("ssid", "", payload, sizeof(payload)));

  constexpr char kToken[] = "0123456789ABCDEF0123456789ABCDEF";
  assert(make_control_url_qr_payload("192.168.4.1", kToken, payload,
                                     sizeof(payload)));
  assert(std::strcmp(
             payload,
             "http://192.168.4.1/#token=0123456789ABCDEF0123456789ABCDEF") ==
         0);
  assert(std::strstr(payload, "?token=") == nullptr);
  assert(!make_control_url_qr_payload("host.local", kToken, payload,
                                      sizeof(payload)));
  assert(!make_control_url_qr_payload("192.168.4.1", "short", payload,
                                      sizeof(payload)));

  char too_small[24]{};
  assert(!make_control_url_qr_payload("192.168.4.1", kToken, too_small,
                                      sizeof(too_small)));
  std::fprintf(stderr, "wifi_ap_web_qr: ok\n");
  return 0;
}

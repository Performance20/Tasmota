/*
  xdrv_52_3_berry_webserver.ino - Berry scripting language, webserver module

  Copyright (C) 2021 Stephan Hadinger, Berry language by Guan Wenliang https://github.com/Skiars/berry

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// also includes tcp_client

#ifdef USE_BERRY

#ifdef USE_WEBCLIENT

#include <berry.h>
#include "HttpClientLight.h"
#include "be_sys.h"

// Tasmota extension
extern File * be_get_arduino_file(void *hfile);

String wc_UrlEncode(const String& text) {
  const char hex[] = "0123456789ABCDEF";

	String encoded = "";
	int len = text.length();
	int i = 0;
	while (i < len)	{
		char decodedChar = text.charAt(i++);

    if (('a' <= decodedChar && decodedChar <= 'z') ||
        ('A' <= decodedChar && decodedChar <= 'Z') ||
        ('0' <= decodedChar && decodedChar <= '9') ||
        ('=' == decodedChar)) {
      encoded += decodedChar;
		} else {
      encoded += '%';
			encoded += hex[decodedChar >> 4];
			encoded += hex[decodedChar & 0xF];
    }

	}
	return encoded;
}

/*********************************************************************************************\
 * Int constants
 *********************************************************************************************/
// const be_const_member_t webserver_constants[] = {
//     { "BUTTON_CONFIGURATION", BUTTON_CONFIGURATION },
//     { "BUTTON_INFORMATION", BUTTON_INFORMATION },
//     { "BUTTON_MAIN", BUTTON_MAIN },
//     { "BUTTON_MANAGEMENT", BUTTON_MANAGEMENT },
//     { "BUTTON_MODULE", BUTTON_MODULE },
//     { "HTTP_ADMIN", HTTP_ADMIN },
//     { "HTTP_ANY", HTTP_ANY },
//     { "HTTP_GET", HTTP_GET },
//     { "HTTP_MANAGER", HTTP_MANAGER },
//     { "HTTP_MANAGER_RESET_ONLY", HTTP_MANAGER_RESET_ONLY },
//     { "HTTP_OFF", HTTP_OFF },
//     { "HTTP_OPTIONS", HTTP_OPTIONS },
//     { "HTTP_POST", HTTP_POST },
//     { "HTTP_USER", HTTP_USER },
// };

/*********************************************************************************************\
 * Native functions mapped to Berry functions
 * 
 * import webclient
 * 
\*********************************************************************************************/
extern "C" {
  // Berry: ``
  //
  int32_t wc_init(struct bvm *vm);
  int32_t wc_init(struct bvm *vm) {
    WiFiClient * wcl = new WiFiClient();
    be_pushcomptr(vm, (void*) wcl);
    be_setmember(vm, 1, ".w");
    HTTPClientLight * cl = new HTTPClientLight();
    cl->setUserAgent(USE_BERRY_WEBCLIENT_USERAGENT);
    cl->setConnectTimeout(USE_BERRY_WEBCLIENT_TIMEOUT);   // set default timeout
    be_pushcomptr(vm, (void*) cl);
    be_setmember(vm, 1, ".p");
    be_return_nil(vm);
  }

  int32_t wc_tcp_init(struct bvm *vm);
  int32_t wc_tcp_init(struct bvm *vm) {
    WiFiClient * wcl = new WiFiClient();
    be_pushcomptr(vm, (void*) wcl);
    be_setmember(vm, 1, ".w");
    be_return_nil(vm);
  }

  HTTPClientLight * wc_getclient(struct bvm *vm) {
    be_getmember(vm, 1, ".p");
    void *p = be_tocomptr(vm, -1);
    be_pop(vm, 1);
    return (HTTPClientLight*) p;
  }

  WiFiClient * wc_getwificlient(struct bvm *vm) {
    be_getmember(vm, 1, ".w");
    void *p = be_tocomptr(vm, -1);
    be_pop(vm, 1);
    return (WiFiClient*) p;
  }

  int32_t wc_deinit(struct bvm *vm);
  int32_t wc_deinit(struct bvm *vm) {
    // int32_t argc = be_top(vm); // Get the number of arguments
    HTTPClientLight * cl = wc_getclient(vm);
    if (cl != nullptr) { delete cl; }
    be_pushnil(vm);
    be_setmember(vm, 1, ".p");
    WiFiClient * wcl = wc_getwificlient(vm);
    if (wcl != nullptr) { delete wcl; }
    be_setmember(vm, 1, ".w");
    be_return_nil(vm);
  }

  int32_t wc_tcp_deinit(struct bvm *vm);
  int32_t wc_tcp_deinit(struct bvm *vm) {
    WiFiClient * wcl = wc_getwificlient(vm);
    if (wcl != nullptr) { delete wcl; }
    be_setmember(vm, 1, ".w");
    be_return_nil(vm);
  }

  // wc.url_encode(string) -> string
  int32_t wc_urlencode(struct bvm *vm);
  int32_t wc_urlencode(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 2 && be_isstring(vm, 2)) {
      const char * s = be_tostring(vm, 2);
      String url = wc_UrlEncode(String(s));
      be_pushstring(vm, url.c_str());
      be_return(vm);  /* return self */
    }
    be_raise(vm, kTypeError, nullptr);
  }

  // wc.begin(url:string) -> self
  int32_t wc_begin(struct bvm *vm);
  int32_t wc_begin(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc == 1 || !be_tostring(vm, 2)) { be_raise(vm, "attribute_error", "missing URL as string"); }
    const char * url = be_tostring(vm, 2);
    HTTPClientLight * cl = wc_getclient(vm);
    // open connection
    if (!cl->begin(url)) {
      be_raise(vm, "value_error", "unsupported protocol");
    }
    be_pushvalue(vm, 1);
    be_return(vm);  /* return self */
  }

  // tcp.connect(address:string, port:int [, timeout_ms:int]) -> bool
  int32_t wc_tcp_connect(struct bvm *vm);
  int32_t wc_tcp_connect(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 3 && be_isstring(vm, 2) && be_isint(vm, 3)) {
      WiFiClient * tcp = wc_getwificlient(vm);
      const char * address = be_tostring(vm, 2);
      int32_t port = be_toint(vm, 3);
      int32_t timeout = USE_BERRY_WEBCLIENT_TIMEOUT;   // default timeout of 2 seconds
      if (argc >= 4) {
        timeout = be_toint(vm, 4);
      }
      // open connection
      bool success = tcp->connect(address, port, timeout);
      be_pushbool(vm, success);
      be_return(vm);  /* return self */
    }
    be_raise(vm, "attribute_error", NULL);
  }

  // wc.close(void) -> nil
  int32_t wc_close(struct bvm *vm);
  int32_t wc_close(struct bvm *vm) {
    HTTPClientLight * cl = wc_getclient(vm);
    cl->end();
    be_return_nil(vm);
  }

  // tcp.close(void) -> nil
  void wc_tcp_close_native(WiFiClient *tcp) {
    tcp->stop();
  }
  int32_t wc_tcp_close(struct bvm *vm) {
    return be_call_c_func(vm, (void*) &wc_tcp_close_native, NULL, ".");
  }

  // tcp.available(void) -> int
  int32_t wc_tcp_available_native(WiFiClient *tcp) {
    return tcp->available();
  }
  int32_t wc_tcp_available(struct bvm *vm) {
    return be_call_c_func(vm, (void*) &wc_tcp_available_native, "i", ".");
  }

  // wc.wc_set_timeouts([http_timeout_ms:int, tcp_timeout_ms:int]) -> self
  int32_t wc_set_timeouts(struct bvm *vm);
  int32_t wc_set_timeouts(struct bvm *vm) {
    int32_t argc = be_top(vm);
    HTTPClientLight * cl = wc_getclient(vm);
    if (argc >= 2) {
      cl->setTimeout(be_toint(vm, 2));
    }
    if (argc >= 3) {
      cl->setConnectTimeout(be_toint(vm, 3));
    }
    be_pushvalue(vm, 1);
    be_return(vm);  /* return self */
  }

  // wc.set_useragent(user_agent:string) -> self
  int32_t wc_set_useragent(struct bvm *vm);
  int32_t wc_set_useragent(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 2 && be_isstring(vm, 2)) {
      HTTPClientLight * cl = wc_getclient(vm);
      const char * useragent = be_tostring(vm, 2);
      cl->setUserAgent(String(useragent));
      be_pushvalue(vm, 1);
      be_return(vm);  /* return self */
    }
    be_raise(vm, kTypeError, nullptr);
  }

  // wc.wc_set_auth(auth:string | (user:string, password:string)) -> self
  int32_t wc_set_auth(struct bvm *vm);
  int32_t wc_set_auth(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 2 && be_isstring(vm, 2) && (argc < 3 || be_isstring(vm, 3))) {
      HTTPClientLight * cl = wc_getclient(vm);
      const char * user = be_tostring(vm, 2);
      if (argc == 2) {
        cl->setAuthorization(user);
      } else {
        const char * password = be_tostring(vm, 3);
        cl->setAuthorization(user, password);
      }
      be_pushvalue(vm, 1);
      be_return(vm);  /* return self */
    }
    be_raise(vm, kTypeError, nullptr);
  }

  // wc.addheader(name:string, value:string [, first:bool=false [, replace:bool=true]]) -> nil
  int32_t wc_addheader(struct bvm *vm);
  int32_t wc_addheader(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 3 && (be_isstring(vm, 2) || be_isstring(vm, 2))) {
      HTTPClientLight * cl = wc_getclient(vm);

      const char * name = be_tostring(vm, 2);
      const char * value = be_tostring(vm, 3);
      bool first = false;
      bool replace = true;
      if (argc >= 4) {
        first = be_tobool(vm, 4);
      }
      if (argc >= 5) {
        replace = be_tobool(vm, 5);
      }
      // do the call
      cl->addHeader(String(name), String(value), first, replace);
      be_return_nil(vm);
    }
    be_raise(vm, kTypeError, nullptr);
  }

  // cw.connected(void) -> bool
  bbool wc_connected_native(HTTPClientLight *cl) {
    return cl->connected();
  }
  int32_t wc_connected(struct bvm *vm) {
    return be_call_c_func(vm, (void*) &wc_connected_native, "b", ".");
  }

  // tcp.connected(void) -> bool
  bbool wc_tcp_connected_native(WiFiClient *tcp) {
    return tcp->connected();
  }
  int32_t wc_tcp_connected(struct bvm *vm) {
    return be_call_c_func(vm, (void*) &wc_tcp_connected_native, "b", ".");
  }

  // tcp.write(bytes | string) -> int
  int32_t wc_tcp_write(struct bvm *vm);
  int32_t wc_tcp_write(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 2 && (be_isstring(vm, 2) || be_isbytes(vm, 2))) {
      WiFiClient * tcp = wc_getwificlient(vm);
      const char * buf = nullptr;
      size_t buf_len = 0;
      if (be_isstring(vm, 2)) {  // string
        buf = be_tostring(vm, 2);
        buf_len = strlen(buf);
      } else { // bytes
        buf = (const char*) be_tobytes(vm, 2, &buf_len);
      }
      size_t bw = tcp->write(buf, buf_len);
      be_pushint(vm, bw);
      be_return(vm);  /* return code */
    }
    be_raise(vm, kTypeError, nullptr);
  }

  // tcp.read() -> string
  int32_t wc_tcp_read(struct bvm *vm);
  int32_t wc_tcp_read(struct bvm *vm) {
    WiFiClient * tcp = wc_getwificlient(vm);
    int32_t btr = tcp->available();
    if (btr <= 0) {
      be_pushstring(vm, "");
    } else {
      char * buf = (char*) be_pushbuffer(vm, btr);
      int32_t btr2 = tcp->read((uint8_t*) buf, btr);
      be_pushnstring(vm, buf, btr2);
    }
    be_return(vm);  /* return code */
  }

  // tcp.readbytes() -> bytes
  int32_t wc_tcp_readbytes(struct bvm *vm);
  int32_t wc_tcp_readbytes(struct bvm *vm) {
    WiFiClient * tcp = wc_getwificlient(vm);
    int32_t btr = tcp->available();
    if (btr <= 0) {
      be_pushbytes(vm, nullptr, 0);
    } else {
      uint8_t * buf = (uint8_t*) be_pushbuffer(vm, btr);
      int32_t btr2 = tcp->read(buf, btr);
      be_pushbytes(vm, buf, btr2);
    }
    be_return(vm);  /* return code */
  }

  void wc_errorCodeMessage(int32_t httpCode, uint32_t http_connect_time) {
    if (httpCode < 0) {
      if (httpCode <= -1000) {
        AddLog(LOG_LEVEL_INFO, D_LOG_HTTP "TLS connection error %d after %d ms", -httpCode - 1000, millis() - http_connect_time);
      } else if (httpCode == -1) {
        AddLog(LOG_LEVEL_INFO, D_LOG_HTTP "Connection timeout after %d ms", httpCode, millis() - http_connect_time);
      } else {
        AddLog(LOG_LEVEL_INFO, D_LOG_HTTP "Connection error %d after %d ms", httpCode, millis() - http_connect_time);
      }
    } else {
      AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_HTTP "Connected in %d ms, stack low mark %d"),
        millis() - http_connect_time, uxTaskGetStackHighWaterMark(nullptr));
    }
  }

  // cw.GET(void) -> httpCode:int
  int32_t wc_GET_native(HTTPClientLight *cl) {
    uint32_t http_connect_time = millis();
    int32_t httpCode = cl->GET();
    wc_errorCodeMessage(httpCode, http_connect_time);
    return httpCode;
  }
  int32_t wc_GET(struct bvm *vm) {
    return be_call_c_func(vm, (void*) &wc_GET_native, "i", ".");
  }

  // wc.POST(string | bytes) -> httpCode:int
  int32_t wc_POST(struct bvm *vm);
  int32_t wc_POST(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 2 && (be_isstring(vm, 2) || be_isbytes(vm, 2))) {
      HTTPClientLight * cl = wc_getclient(vm);
      const char * buf = nullptr;
      size_t buf_len = 0;
      if (be_isstring(vm, 2)) {  // string
        buf = be_tostring(vm, 2);
        buf_len = strlen(buf);
      } else { // bytes
        buf = (const char*) be_tobytes(vm, 2, &buf_len);
      }
      uint32_t http_connect_time = millis();
      int32_t httpCode = cl->POST((uint8_t*)buf, buf_len);
      wc_errorCodeMessage(httpCode, http_connect_time);
      be_pushint(vm, httpCode);
      be_return(vm);  /* return code */
    }
    be_raise(vm, kTypeError, nullptr);
  }

  int32_t wc_getstring(struct bvm *vm);
  int32_t wc_getstring(struct bvm *vm) {
    HTTPClientLight * cl = wc_getclient(vm);
    int32_t sz = cl->getSize();
    // abort if we exceed 32KB size, things will not go well otherwise
    if (sz >= 32767) {
      be_raise(vm, "value_error", "response size too big (>32KB)");
    }
    String payload = cl->getString();
    be_pushstring(vm, payload.c_str());
    cl->end();  // free allocated memory ~16KB
    be_return(vm);  /* return code */
  }

  int32_t wc_writefile(struct bvm *vm);
  int32_t wc_writefile(struct bvm *vm) {
    int32_t argc = be_top(vm);
    if (argc >= 2 && be_isstring(vm, 2)) {
      HTTPClientLight * cl = wc_getclient(vm);
      const char * fname = be_tostring(vm, 2);

      void * f = be_fopen(fname, "w");
      int ret = -1;
      if (f) {
        File * fptr = be_get_arduino_file(f);
        ret = cl->writeToStream(fptr);
      }
      be_fclose(f);
      be_pushint(vm, ret);
      be_return(vm);  /* return code */
    }
    be_raise(vm, kTypeError, nullptr);
  }

  int32_t wc_getsize_native(HTTPClientLight *cl) {
    return cl->getSize();
  }
  int32_t wc_getsize(struct bvm *vm) {
    return be_call_c_func(vm, (void*) &wc_getsize_native, "i", ".");
  }

}

#endif // USE_WEBCLIENT
#endif  // USE_BERRY

#ifndef ASYNC_FS_WEBSERVER_H
#define ASYNC_FS_WEBSERVER_H

#include <FS.h>
#include <DNSServer.h>
#include "ESPAsyncWebServer/src/ESPAsyncWebServer.h"

#ifdef ESP32
  #include <Update.h>
  #include <ESPmDNS.h>
  #include "esp_wifi.h"
  #include "esp_task_wdt.h"
  #include "sys/stat.h"
#elif defined(ESP8266)
  #include <ESP8266mDNS.h>
  #include <Updater.h>
#else
  #error Platform not supported
#endif

#define INCLUDE_EDIT_HTM
#ifdef INCLUDE_EDIT_HTM
#include "edit_htm.h"
#endif

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include "setup_htm.h"
#define CONFIG_FOLDER "/config"
#define CONFIG_FILE "/config.json"

#define DBG_OUTPUT_PORT     Serial
#define LOG_LEVEL           2         // (0 disable, 1 error, 2 info, 3 debug)
#include "SerialLog.h"
#include "CaptivePortal.hpp"
#include "SetupConfig.hpp"

#define MIN_F -3.4028235E+38
#define MAX_F 3.4028235E+38
#define MAX_APNAME_LEN 16

typedef struct {
  size_t totalBytes;
  size_t usedBytes;
  char fsName[MAX_APNAME_LEN];
} fsInfo_t;

using FsInfoCallbackF = std::function<void(fsInfo_t*)>;
using CallbackF = std::function<void(void)>;

class AsyncFsWebServer : public AsyncWebServer
{
  protected:
    AsyncWebSocket* m_ws = nullptr;
    AsyncWebHandler *m_captive = nullptr;
    DNSServer* m_dnsServer = nullptr;

    void handleWebSocket(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t * data, size_t len);
    void handleScanNetworks(AsyncWebServerRequest *request);
    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void doWifiConnection(AsyncWebServerRequest *request);

    void notFound(AsyncWebServerRequest *request);
    void handleSetup(AsyncWebServerRequest *request);
    void getStatus(AsyncWebServerRequest *request);
    void clearConfig(AsyncWebServerRequest *request);
    void handleFileName(AsyncWebServerRequest *request);

    // Get data and then do update
    void update_first(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void update_second(AsyncWebServerRequest *request);

        // edit page, in usefull in some situation, but if you need to provide only a web interface, you can disable
#ifdef INCLUDE_EDIT_HTM
    void deleteContent(String& path) ;
    void handleFileDelete(AsyncWebServerRequest *request);
    void handleFileCreate(AsyncWebServerRequest *request);
    void handleFsStatus(AsyncWebServerRequest *request);
    void handleFileList(AsyncWebServerRequest *request);
    void handleFileEdit(AsyncWebServerRequest *request);
#endif

  void setTaskWdt(uint32_t timeout);

  /*
    Add an option which contain "raw" HTML code to be injected in /setup page
    Th HTML code will be written in a file with named as option id
  */
  void addSource(const char* source, const char* tag, bool overWrite = false) ;

  /*
    Create a dir if not exist on uploading files
  */
  bool createDirFromPath( const String& path) ;

  private:
    char* m_pageUser = nullptr;
    char* m_pagePswd = nullptr;
    String m_host = "esphost";
    fs::FS* m_filesystem = nullptr;

    uint32_t m_timeout = 10000;
    uint8_t numOptions = 0;
    char m_version[16] = {__TIME__};
    bool m_filesystem_ok = false;
    char m_apWebpage[MAX_APNAME_LEN] = "/setup";
    size_t m_contentLen = 0;
    uint16_t m_port;
    FsInfoCallbackF getFsInfo = nullptr;

  public:
    SetupConfigurator setup;

    AsyncFsWebServer(uint16_t port, fs::FS &fs, const char* hostname = "") :
    AsyncWebServer(port),
    m_filesystem(&fs),
    setup(&fs),
    m_port(port)
    {
      m_ws = new AsyncWebSocket("/ws");
      if (strlen(hostname))
        m_host = hostname;
    }

    ~AsyncFsWebServer() {
      reset();
      end();
      if(_catchAllHandler) delete _catchAllHandler;
    }

  #ifdef ESP32
    inline TaskHandle_t getTaskHandler() {
      return xTaskGetCurrentTaskHandle();
    }
  #endif

    /*
      Start webserver aand bind a websocket event handler (optional)
    */
    bool init(AwsEventHandler wsHandle = nullptr);

    /*
      Enable the built-in ACE web file editor
    */
    void enableFsCodeEditor();

    /*
      Enable authenticate for /setup webpage
    */
    void setAuthentication(const char* user, const char* pswd);

    /*
      List FS content
    */
    void printFileList(fs::FS &fs, const char * dirname, uint8_t levels);

    /*
      Send a default "OK" reply to client
    */
    void sendOK(AsyncWebServerRequest *request);

    /*
      Start WiFi connection, if fails to in AP mode
    */
    IPAddress startWiFi(uint32_t timeout, const char *apSSID, const char *apPsw, CallbackF fn=nullptr);

    /*
      Start WiFi connection, NO AP mode on fail
    */
    IPAddress startWiFi(uint32_t timeout, CallbackF fn=nullptr ) ;

    /*
     * Redirect to captive portal if we got a request for another domain.
    */
    bool startCaptivePortal(const char* ssid, const char* pass, const char* redirectTargetURL);


    /*
     * get instance of current websocket handler
    */
    AsyncWebSocket* getWebSocket() { return m_ws;}

    /*
     * Broadcast a websocket message to all clients connected
    */
    void wsBroadcast(const char * buffer) {
      m_ws->textAll(buffer);
    }

    /*
    * Need to be run in loop to handle DNS requests
    */
    void updateDNS() {
      m_dnsServer->processNextRequest();
    }

    /*
    * Set callback function to provide updated FS info to library
    * This it is necessary due to the different implementation of
    * libraries for the filesystem (LittleFS, FFat, SPIFFS etc etc)
    */
    void setFsInfoCallback(FsInfoCallbackF fsCallback) {
      getFsInfo = fsCallback;
    }

    /*
    * Get reference to current config.json file
    */
    File getConfigFile(const char* mode) {
      File file = m_filesystem->open(CONFIG_FOLDER CONFIG_FILE, mode);
      return file;
    }

    /*
    * Get complete path of config.json file
    */
    const char* getConfiFileName() {
      return CONFIG_FOLDER CONFIG_FILE;
    }

    /*
    * Set current firmware version (shown in /setup webpage)
    */
    void setFirmwareVersion(char* version) {
      strlcpy(m_version, version, sizeof(m_version));
    }

    /*
    * Set current library version
    */
    const char* getVersion();

    /*
    * Set /setup webpage title
    */
    void setSetupPageTitle(const char* title) {
      setup.addOption("name-logo", title);
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////   BACKWARD COMPATIBILITY ONLY /////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////
    bool optionToFile(const char* f, const char* id, bool ow) {return setup.optionToFile(f, id, ow);}
    void addHTML(const char* h, const char* id, bool ow = false) {setup.addHTML(h, id, ow);}
    void addCSS(const char* c, const char* id, bool ow = false){setup.addCSS(c, id, ow);}
    void addJavascript(const char* s, const char* id, bool ow = false) {setup.addJavascript(s, id, ow);}
    void addDropdownList(const char *l, const char** a, size_t size){setup.addDropdownList(l, a, size);}
    void addOptionBox(const char* title) { setup.addOption("param-box", title); }
    void setLogoBase64(const char* logo, const char* w = "128", const char* h = "128", bool ow = false) {
      setup.setLogoBase64(logo, w, h, ow);
    }
    template <typename T>
    void addOption(const char *lbl, T val, double min, double max, double st){
      setup.addOption(lbl, val, false, min, max, st);
    }
    template <typename T>
    void addOption(const char *lbl, T val, bool hd = false,  double min = MIN_F,
      double max = MAX_F, double st = 1.0) {
      setup.addOption(lbl, val, hd, min, max, st);
    }
    template <typename T>
    bool getOptionValue(const char *lbl, T &var) { return setup.getOptionValue(lbl, var);}
    /////////////////////////////////////////////////////////////////////////////////////////////////

};

#endif

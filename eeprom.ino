// WiFi.mode(m): выбрать режим WIFI_AP (точка доступа), WIFI_STA (клиент), или WIFI_AP_STA (оба режима одновременно).
// WiFi.softAP(ssid) создает открытую точку доступа
// WiFi.softAP(ssid, password) создает точку доступа с WPA2-PSK шифрованием, пароль должен быть не менее 8 символов

void cws()
{

   IPAddress ip = WiFi.softAPIP();
   IPAddress ipl = WiFi.localIP();

server.on("/favicon.ico", []()
      {
        server.send_P(200, "image/x-icon", PAGE_favicon, sizeof(PAGE_favicon));
      });
    
server.on("/poweron", []()
      {
        digitalWrite(PowerRelay, HIGH);
        server.send(200, "text/html", "poweron");
      });


server.on("/poweroff", []()
      {
        digitalWrite(PowerRelay, LOW);
        server.send(200, "text/html", "poweroff");
      });
    


server.on("/Restart", []() {
      
      Serial.println("Restarting...");
      
      content = "<html> <head> <meta charset='utf-8'> </head>";
      content += "<p>Restarting...</p>";
      content += "</html>";
      server.send(200, "text/html", content);

      ESP.restart();
     
    });


server.on("/", []() {

      IPAddress ip = WiFi.softAPIP();
      IPAddress ipl = WiFi.localIP();
      
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      String ipStrl = String(ipl[0]) + '.' + String(ipl[1]) + '.' + String(ipl[2]) + '.' + String(ipl[3]);
      
String content = "";

//content = R"=="==(
//)=="==";
//    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\
//<meta http-equiv=\"refresh\" content=\"600\">\

content = "<!DOCTYPE html><html><head>\
    <meta charset=\"utf-8\" />\
    <title>Dronsky BLE adv receiver from Xiaomi Mijia (LYWSD03MMC)</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    .ok{\
      background: #df922f;\
      font-family: Tahoma;\
      color: #ffffff;\
      border:0;\
      font-weight: bold;\
      border-radius: 5px;\
      width: auto;\
      height: auto;\
      font-size:110%;\
}\
 p.dline {\
    line-height: 0.7;\
     margin-top: 0px;\
   }\
.ok:hover {\
  background: #1d49aa;\
}\
.ok:focus {\
  outline: none;\
  box-shadow: 0 0 0 4px #cbd6ee;\
}\
</style>\
  </head>\
  <link rel=\"shortcut icon\" href=\"/favicon.ico\" type=\"image/x-icon\">";

  String scr="";

  scr = R"=="==(
        <script>
        var ble = "";
        var last_update = "";
        var cd = 60;

        function GetArduinoInputs()
        {
            var request = new XMLHttpRequest();
            request.onreadystatechange = function()
            {
                if (this.readyState == 4) {
                   if (this.status == 200) {
                        if (this.response != null) { //XML

                          ble = this.response;
//                            ble = this.responseXML.getElementsByTagName('analog')[0].childNodes[0].nodeValue;
//                            last_update = this.responseXML.getElementsByTagName('analog')[1].childNodes[0].nodeValue;

                            document.getElementById('ble').innerHTML = ble;
                            cd = 60;
//                            document.getElementById('last_update').innerHTML = last_update;
 
                        }
                    }
                }
            }
            request.open("GET", "ajax_inputs", true);
            request.send(null);
            setTimeout('GetArduinoInputs()', 60000); //600000
        }
        
        setInterval(function() {
          cd--;
          document.getElementById('cd').innerHTML = cd;
          document.getElementById('time').innerHTML = date_time();
         }, 1000);

    /* функция добавления ведущих нулей */
    /* (если число меньше десяти, перед числом добавляем ноль) */
    function zero_first_format(value)
    {
        if (value < 10)
        {
            value='0'+value;
        }
        return value;
    }

    /* функция получения текущей даты и времени */
    function date_time()
    {
        var current_datetime = new Date();
        var day = zero_first_format(current_datetime.getDate());
        var month = zero_first_format(current_datetime.getMonth()+1);
        var year = current_datetime.getFullYear();
        var hours = zero_first_format(current_datetime.getHours());
        var minutes = zero_first_format(current_datetime.getMinutes());
        var seconds = zero_first_format(current_datetime.getSeconds());
//day+"."+month+"."+year+" "+
        return day+"."+month+"."+year+" "+hours+":"+minutes+":"+seconds;
    }
    </script>
)=="==";

content += scr;
    
content += "<body onload='GetArduinoInputs()'>";

content += " <p class='dline'><small><small>";

content += "Dronsky BLE adv receiver from Xiaomi Mijia (LYWSD03MMC).<br>Based on the pvvx work: <a href=\"https://github.com/pvvx/ATC_MiThermometer/tree/master/esp32\">https://github.com/pvvx/ATC_MiThermometer/tree/master/esp32</a><br>Версия: ";

content += c + "<br></small></small></p>";

content += " <span id='ble'>" + sBLEdata + "</span>";

content += "<span id='time'>00:00:00</span><br>"; //U: <span id='cd'>600</span> 

    //)=="==";

content += "\
<table style='border-collapse: collapse; border-color: red; background-color: yellow; width: 50%;' border='1'\
cellspacing='10' cellpadding='10'><caption>Управление системой:</caption>\
<tbody>\
<td style='width: auto;'><center><input class='ok' type='button' onclick='location.reload();' value='Обновить'/></center></td>\
<td style='width: auto;'><center><form action='Restart' method='get'><input class='ok' type='submit' value='Restart'></form></center></td>\
</tr>\
</tbody>\
</table>\
"; 

     yield();

if(WiFi.isConnected()) {
  content += "<p>Connected to: " + WiFi.SSID() + ", IP: " + ipStrl + ", DNS name: " + http_name + "</p>";
}

   content += "<small><small><br>(c) Сергей Дронский 2023</small></small>";
       
   content += "</body></html>";
   
   server.send(200, "text/html", content);
    });

server.on("/ajax_inputs", []() {
      server.send(200, "text/xml", sBLEdata);
    });

}

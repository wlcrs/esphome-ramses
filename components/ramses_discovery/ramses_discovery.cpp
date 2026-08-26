#include "ramses_discovery.h"
#include "esphome/core/log.h"
#include <iomanip>
#include <sstream>

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#include "esphome/core/hal.h"
#endif

namespace esphome {
namespace ramses_discovery {

static const char *const TAG = "ramses_discovery";

static const char RAMSES_DISCOVERY_INDEX_HTML[] = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RAMSES II Discovery Dashboard</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --card-border: #334155;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --accent: #38bdf8;
      --accent-hover: #0ea5e9;
      --accent-grad: linear-gradient(135deg, #38bdf8 0%, #818cf8 100%);
      --success: #34d399;
      --warning: #fbbf24;
      --danger: #f87171;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text);
      line-height: 1.5;
      padding: 1.5rem;
    }
    .container { max-width: 1100px; margin: 0 auto; }
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 2rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid var(--card-border);
      flex-wrap: wrap;
      gap: 1rem;
    }
    .title-group { display: flex; align-items: center; gap: 0.75rem; }
    .icon { font-size: 1.75rem; }
    h1 {
      font-size: 1.5rem;
      font-weight: 700;
      background: var(--accent-grad);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .status-pill {
      font-size: 0.75rem;
      font-weight: 600;
      padding: 0.25rem 0.6rem;
      border-radius: 9999px;
      background: rgba(52, 211, 153, 0.15);
      color: var(--success);
      display: inline-flex;
      align-items: center;
      gap: 0.35rem;
    }
    .status-dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: var(--success);
      animation: pulse 2s infinite;
    }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
    .actions-bar { display: flex; gap: 0.75rem; align-items: center; }
    button {
      background: #2563eb;
      color: #fff;
      border: none;
      padding: 0.5rem 1rem;
      border-radius: 0.375rem;
      font-size: 0.875rem;
      font-weight: 500;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 0.4rem;
      transition: background 0.2s ease, transform 0.1s;
    }
    button:hover { background: #1d4ed8; }
    button:active { transform: scale(0.98); }
    button.secondary { background: var(--card-border); color: var(--text); }
    button.secondary:hover { background: #475569; }
    button.small { padding: 0.3rem 0.6rem; font-size: 0.75rem; }
    
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 0.75rem;
      overflow: hidden;
      margin-bottom: 2rem;
      box-shadow: 0 4px 6px -1px rgba(0,0,0,0.2);
    }
    .card-header {
      padding: 1rem 1.25rem;
      background: rgba(255,255,255,0.02);
      border-bottom: 1px solid var(--card-border);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .card-title { font-weight: 600; font-size: 1.1rem; }
    
    table { width: 100%; border-collapse: collapse; text-align: left; }
    th {
      padding: 0.75rem 1.25rem;
      background: rgba(0,0,0,0.15);
      color: var(--text-muted);
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      border-bottom: 1px solid var(--card-border);
    }
    td {
      padding: 1rem 1.25rem;
      border-bottom: 1px solid var(--card-border);
      font-size: 0.875rem;
      vertical-align: middle;
    }
    tr:last-child td { border-bottom: none; }
    tr:hover td { background: rgba(255,255,255,0.02); }
    
    .addr-badge {
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-weight: 600;
      background: rgba(56, 189, 248, 0.12);
      color: var(--accent);
      padding: 0.25rem 0.5rem;
      border-radius: 0.25rem;
      font-size: 0.85rem;
    }
    .badge {
      display: inline-block;
      padding: 0.2rem 0.5rem;
      border-radius: 0.25rem;
      font-size: 0.75rem;
      font-weight: 600;
      text-transform: capitalize;
    }
    .badge-controller { background: rgba(129, 140, 248, 0.2); color: #a5b4fc; }
    .badge-hvac { background: rgba(52, 211, 153, 0.2); color: #6ee7b7; }
    .badge-remote { background: rgba(59, 130, 246, 0.2); color: #93c5fd; }
    .badge-trv { background: rgba(251, 191, 36, 0.2); color: #fde68a; }
    .badge-opentherm { background: rgba(248, 113, 113, 0.2); color: #fca5a5; }
    .badge-sensor { background: rgba(148, 163, 184, 0.2); color: #cbd5e1; }

    
    .zones-list { margin-top: 0.35rem; display: flex; flex-direction: column; gap: 0.2rem; }
    .zone-item { font-size: 0.8rem; color: var(--text-muted); }
    .zone-name { color: var(--text); font-weight: 500; }
    
    .rssi-pill {
      font-family: monospace;
      font-weight: 600;
      font-size: 0.8rem;
    }
    .rssi-good { color: var(--success); }
    .rssi-fair { color: var(--warning); }
    .rssi-poor { color: var(--danger); }
    
    pre {
      background: #090d16;
      padding: 1.25rem;
      border-radius: 0.5rem;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-size: 0.8rem;
      color: #e2e8f0;
      overflow-x: auto;
      white-space: pre;
      max-height: 400px;
      line-height: 1.4;
    }
    
    .toast {
      position: fixed;
      bottom: 2rem;
      right: 2rem;
      background: #10b981;
      color: #ffffff;
      padding: 0.75rem 1.25rem;
      border-radius: 0.5rem;
      box-shadow: 0 10px 15px -3px rgba(0,0,0,0.3);
      font-size: 0.875rem;
      font-weight: 500;
      display: none;
      z-index: 1000;
      animation: fadeIn 0.2s ease;
    }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
    .empty-state { text-align: center; padding: 3rem; color: var(--text-muted); }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="title-group">
        <div class="icon">📡</div>
        <div>
          <h1>RAMSES II Auto-Discovery</h1>
          <div style="display: flex; align-items: center; gap: 0.5rem; margin-top: 0.25rem;">
            <span class="status-pill"><span class="status-dot"></span> Sniffer Active</span>
            <span id="device-count" style="font-size: 0.8rem; color: var(--text-muted);">0 devices detected</span>
          </div>
        </div>
      </div>
      <div class="actions-bar">
        <button id="btn-probe" class="secondary" onclick="triggerProbe()">⚡ Trigger Probing</button>
        <button onclick="copyFullYaml()">📋 Copy Full Configuration</button>
      </div>
    </header>

    <div class="card">
      <div class="card-header">
        <span class="card-title">Discovered Devices</span>
        <span id="last-update" style="font-size: 0.75rem; color: var(--text-muted);">Auto-updating...</span>
      </div>
      <div style="overflow-x: auto;">
        <table id="device-table">
          <thead>
            <tr>
              <th>Address</th>
              <th>Type / OEM</th>
              <th>Details & Zones</th>
              <th>Signal</th>
              <th>Last Seen</th>
              <th style="text-align: right;">Action</th>
            </tr>
          </thead>
          <tbody id="device-tbody">
            <tr>
              <td colspan="6" class="empty-state">
                Listening for RAMSES II RF packets (Evohome, MVHR, TRVs, OpenTherm)...
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <span class="card-title">Generated ESPHome Configuration (YAML)</span>
        <button class="small secondary" onclick="copyFullYaml()">Copy YAML</button>
      </div>
      <div style="padding: 1rem;">
        <pre id="yaml-preview"># Waiting for devices to be discovered...</pre>
      </div>
    </div>
  </div>

  <div id="toast" class="toast">YAML copied to clipboard!</div>

  <script>
    let currentData = { devices: [], full_yaml: "" };

    function showToast(msg) {
      const t = document.getElementById("toast");
      t.innerText = msg;
      t.style.display = "block";
      setTimeout(() => { t.style.display = "none"; }, 2500);
    }

    function copyToClipboard(text, msg = "YAML copied to clipboard!") {
      navigator.clipboard.writeText(text).then(() => showToast(msg));
    }

    function copyDeviceYaml(index) {
      if (currentData.devices && currentData.devices[index]) {
        copyToClipboard(currentData.devices[index].yaml, "Device YAML copied!");
      }
    }

    function copyFullYaml() {
      if (currentData.full_yaml) {
        copyToClipboard(currentData.full_yaml, "Full YAML copied!");
      }
    }

    function triggerProbe() {
      const btn = document.getElementById("btn-probe");
      btn.innerText = "⏳ Probing...";
      btn.disabled = true;
      fetch("/ramses/probe", { method: "POST" })
        .then(() => {
          showToast("Probing queries sent (RQ 0004 / RQ 10E0)!");
          setTimeout(fetchData, 1000);
        })
        .catch(() => showToast("Probe request completed"))
        .finally(() => {
          setTimeout(() => {
            btn.innerText = "⚡ Trigger Probing";
            btn.disabled = false;
          }, 2000);
        });
    }

    function renderDevices(data) {
      currentData = data;
      const tbody = document.getElementById("device-tbody");
      const devCount = document.getElementById("device-count");
      const yamlPre = document.getElementById("yaml-preview");

      devCount.innerText = `${data.devices ? data.devices.length : 0} device(s) detected`;
      yamlPre.innerText = data.full_yaml || "# No devices detected yet.";

      if (!data.devices || data.devices.length === 0) {
        tbody.innerHTML = `<tr><td colspan="6" class="empty-state">Listening for RAMSES II RF packets...</td></tr>`;
        return;
      }

      let html = "";
      data.devices.forEach((dev, idx) => {
        let badgeClass = "badge-sensor";
        if (dev.device_type === "controller") badgeClass = "badge-controller";
        else if (dev.device_type === "hvac") badgeClass = "badge-hvac";
        else if (dev.device_type === "remote") badgeClass = "badge-remote";
        else if (dev.device_type === "trv") badgeClass = "badge-trv";
        else if (dev.device_type === "opentherm") badgeClass = "badge-opentherm";

        let rssiClass = "rssi-good";
        if (dev.rssi < -85) rssiClass = "rssi-poor";
        else if (dev.rssi < -70) rssiClass = "rssi-fair";

        let details = "";
        if (dev.zones && dev.zones.length > 0) {
          details += `<div class="zones-list">`;
          dev.zones.forEach(z => {
            let tStr = z.temp !== undefined ? ` • ${z.temp.toFixed(1)}°C` : "";
            let sStr = z.setpoint !== undefined ? ` (Target ${z.setpoint.toFixed(1)}°C)` : "";
            details += `<div class="zone-item"><span class="zone-name">${z.name || "Zone " + z.index}</span>${tStr}${sStr}</div>`;
          });
          if (dev.has_dhw) {
            details += `<div class="zone-item"><span class="zone-name">DHW Cylinder</span> • Configured</div>`;
          }
          details += `</div>`;
        } else if (dev.associated_remote) {
          details = `<span style="color: #6ee7b7; font-size: 0.82rem; font-weight: 500;">🔗 Cloned Remote: <strong>${dev.associated_remote}</strong></span>`;
          if (dev.oem_name && dev.oem_name !== "generic") {
            details += `<br><span style="color: var(--text-muted); font-size: 0.75rem;">OEM Scheme: ${dev.oem_name}</span>`;
          }
        } else if (dev.associated_target) {
          details = `<span style="color: #93c5fd; font-size: 0.82rem;">🎮 Controls MVHR: <strong>${dev.associated_target}</strong></span>`;
        } else if (dev.oem_name && dev.oem_name !== "generic") {
          details = `<span style="color: var(--text-muted); font-size: 0.8rem;">OEM Scheme: <strong style="color: var(--text);">${dev.oem_name}</strong></span>`;
        } else {
          details = `<span style="color: var(--text-muted); font-size: 0.8rem;">Standard RAMSES device</span>`;
        }


        let seenStr = dev.last_seen_sec <= 2 ? "Just now" : `${dev.last_seen_sec}s ago`;

        html += `<tr>
          <td><span class="addr-badge">${dev.address}</span></td>
          <td><span class="badge ${badgeClass}">${dev.type_label}</span></td>
          <td>${details}</td>
          <td><span class="rssi-pill ${rssiClass}">${dev.rssi !== 0 ? dev.rssi + " dBm" : "N/A"}</span></td>
          <td style="color: var(--text-muted);">${seenStr}</td>
          <td style="text-align: right;">
            <button class="small secondary" onclick="copyDeviceYaml(${idx})">Copy YAML</button>
          </td>
        </tr>`;
      });

      tbody.innerHTML = html;
      document.getElementById("last-update").innerText = `Updated ${new Date().toLocaleTimeString()}`;
    }

    function fetchData() {
      fetch("/ramses/devices.json")
        .then(res => res.json())
        .then(data => renderDevices(data))
        .catch(err => {
          console.debug("Fetch error:", err);
        });
    }

    fetchData();
    setInterval(fetchData, 3000);
  </script>
</body>
</html>
)rawhtml";

static std::string json_escape(const std::string &input) {
  std::string output;
  output.reserve(input.size() + 16);
  for (char c : input) {
    switch (c) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\b':
      output += "\\b";
      break;
    case '\f':
      output += "\\f";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        output += buf;
      } else {
        output += c;
      }
      break;
    }
  }
  return output;
}

#ifdef RAMSES_HAS_WEB_SERVER_BASE
class RamsesWebHandler : public AsyncWebHandler {
public:
  explicit RamsesWebHandler(RamsesDiscoveryComponent *parent)
      : parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    StringRef u = request->url_to(url_buf);
    return u == "/ramses" || u == "/discovery" || u == "/ramses/devices.json" ||
           u == "/ramses/probe";
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    StringRef u = request->url_to(url_buf);
    if (u == "/ramses" || u == "/discovery") {
      request->send(200, "text/html", RAMSES_DISCOVERY_INDEX_HTML);
    } else if (u == "/ramses/devices.json") {
      uint32_t now = 0;
#ifdef USE_ESP_IDF
      now = millis();
#endif
      std::string json = this->parent_->generate_json(now);
      request->send(200, "application/json", json.c_str());
    } else if (u == "/ramses/probe") {
      this->parent_->trigger_probe();
      request->send(200, "application/json",
                    "{\"status\":\"probing_triggered\"}");
    }
  }

protected:
  RamsesDiscoveryComponent *parent_;
};
#endif

void RamsesDiscoveryComponent::setup() {
  ESP_LOGI(TAG, "Initializing RAMSES Auto-Discovery engine...");
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback(
        [this](const ramses_esp::RamsesMessage &msg) {
          this->on_message(msg);
        });
  }
#endif

#ifdef RAMSES_HAS_WEB_SERVER_BASE
  if (this->web_server_base_ == nullptr &&
      web_server_base::global_web_server_base != nullptr) {
    this->web_server_base_ = web_server_base::global_web_server_base;
  }
  if (this->web_server_base_ != nullptr) {
    this->web_server_base_->add_handler(new RamsesWebHandler(this));
    ESP_LOGI(TAG, "Registered RAMSES Discovery Web UI dashboard at /ramses");
  }
#endif
}

void RamsesDiscoveryComponent::loop() {
  uint32_t now = millis();
  if (this->active_probing_ &&
      (now - this->last_probe_time_ > this->probing_interval_ms_)) {
    this->last_probe_time_ = now;
    this->probe_pending();
  }

  // Periodic discovery summary dump every 60s
  if (now - this->last_dump_time_ > 60000) {
    this->last_dump_time_ = now;
    if (!this->devices_.empty()) {
      this->dump_yaml();
    }
  }
}

void RamsesDiscoveryComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RAMSES Discovery Component:");
  ESP_LOGCONFIG(TAG, "  Active Probing: %s",
                this->active_probing_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Probing Interval: %u ms",
                (unsigned int)this->probing_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Discovered Devices: %d", (int)this->devices_.size());
}

DiscoveredDevice &RamsesDiscoveryComponent::get_or_create_device(
    const ramses_esp::RamsesAddress &addr) {
  std::string key = addr.to_string();
  auto it = this->devices_.find(key);
  if (it != this->devices_.end()) {
    return it->second;
  }

  DiscoveredDevice dev;
  dev.address = addr;

  // Infer device type from prefix
  switch (addr.dev_class) {
  case 1:
    dev.device_type = "controller";
    break;
  case 4:
    dev.device_type = "trv";
    break;
  case 10:
    dev.device_type = "opentherm";
    break;
  case 13:
    dev.device_type = "relay";
    break;
  case 18:
    dev.device_type = "gateway";
    break;
  case 22:
  case 34:
    dev.device_type = "sensor";
    break;
  case 32:
  case 37:
  case 29:
    dev.device_type = "hvac";
    dev.is_hvac = true;
    break;
  default:
    dev.device_type = "other";
    break;
  }

  this->devices_[key] = dev;
  return this->devices_[key];
}

void RamsesDiscoveryComponent::on_message(
    const ramses_esp::RamsesMessage &msg) {
  // 1. Process Source Device (addr[0])
  if (msg.fields & RAMSES_F_ADDR0) {
    ramses_esp::RamsesAddress src =
        ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
    if (src.is_valid && src.dev_class != 63) {
      DiscoveredDevice &dev = this->get_or_create_device(src);
      dev.last_rssi = msg.rssi;
#ifdef USE_ESP_IDF
      dev.last_seen_ms = millis();
#else
      dev.last_seen_ms = 1000;
#endif
      uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
      dev.seen_opcodes.insert(opcode);

      if (dev.device_type == "controller") {
        this->process_controller_packet(dev, msg, opcode);
      } else if (dev.device_type == "hvac" || dev.is_hvac) {
        this->process_hvac_packet(dev, msg, opcode);
      } else if (dev.device_type == "trv") {
        this->process_trv_packet(dev, msg, opcode);
      } else if (dev.device_type == "opentherm") {
        this->process_opentherm_packet(dev, msg, opcode);
      } else {
        this->process_sensor_packet(dev, msg, opcode);
      }
    }
  }

  // 2. Also register Target Device (addr[1]) if present (ignore broadcast and
  // gateways)
  if (msg.fields & RAMSES_F_ADDR1) {
    ramses_esp::RamsesAddress dst =
        ramses_esp::RamsesAddress::from_bytes(msg.addr[1]);
    if (dst.is_valid && dst.dev_class != 63 && dst.dev_class != 18) {
      DiscoveredDevice &dst_dev = this->get_or_create_device(dst);
#ifdef USE_ESP_IDF
      dst_dev.last_seen_ms = millis();
#else
      dst_dev.last_seen_ms = 1000;
#endif
      uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
      dst_dev.seen_opcodes.insert(opcode);

      // If fan command 22F1/22F3 was sent to target, target is the HVAC unit
      if (opcode == 0x22F1 || opcode == 0x22F3) {
        dst_dev.is_hvac = true;
        dst_dev.device_type = "hvac";
        if (msg.fields & RAMSES_F_ADDR0) {
          ramses_esp::RamsesAddress src =
              ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
          if (src.is_valid && src.dev_class != 63 && src.dev_class != 18) {
            dst_dev.associated_remote = src.to_string();
            DiscoveredDevice &src_dev = this->get_or_create_device(src);
            src_dev.device_type = "remote";
            src_dev.is_hvac = false;
            src_dev.associated_target = dst.to_string();
          }
        }
        this->process_hvac_packet(dst_dev, msg, opcode);
      }
    }
  }
}

void RamsesDiscoveryComponent::process_controller_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x30C9) {
    auto dec =
        ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      for (const auto &item : dec->zones) {
        DiscoveredZone &zone = dev.zones[item.zone_index];
        zone.index = item.zone_index;
        zone.last_temp = item.temperature;
        zone.has_temp = true;
      }
    }
  } else if (opcode == 0x2309) {
    auto dec = ramses_esp::SetpointPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      for (const auto &item : dec->zones) {
        DiscoveredZone &zone = dev.zones[item.zone_index];
        zone.index = item.zone_index;
        zone.last_setpoint = item.setpoint;
        zone.has_setpoint = true;
      }
    }
  } else if (opcode == 0x0004) {
    auto dec = ramses_esp::ZoneNamePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      DiscoveredZone &zone = dev.zones[dec->zone_index];
      zone.index = dec->zone_index;
      zone.name = dec->name;
      zone.name_probed = true;
      ESP_LOGI(TAG, "Discovered Zone %u Name: '%s' for Controller %s",
               dec->zone_index, dec->name.c_str(),
               dev.address.to_string().c_str());
    }
  } else if (opcode == 0x1260 || opcode == 0x12F0 || opcode == 0x1F41) {
    dev.has_dhw = true;
  }
}

void RamsesDiscoveryComponent::process_hvac_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x10E0) {
    auto dec =
        ramses_esp::DeviceInfoPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.oem_probed = true;
      if (dec->oem_code == 0x67) {
        dev.oem_name = "orcon";
        dev.hvac_scheme = ramses_esp::HvacScheme::ORCON;
      } else if (dec->oem_code == 0x6A) {
        dev.oem_name = "orcon"; // Hopper D375 / Brofer uses the Orcon scheme
        dev.hvac_scheme = ramses_esp::HvacScheme::ORCON;
      } else if (dec->oem_code == 0x08 || dec->oem_code == 0x01) {
        dev.oem_name = "itho";
        dev.hvac_scheme = ramses_esp::HvacScheme::ITHO;
      } else if (dec->oem_code == 0x13 || dec->oem_code == 0x66) {
        dev.oem_name = "vasco";
        dev.hvac_scheme = ramses_esp::HvacScheme::VASCO;
      } else if (dec->oem_code == 0x02) {
        dev.oem_name = "zehnder";
        dev.hvac_scheme = ramses_esp::HvacScheme::ZEHNDER;
      }
      ESP_LOGI(TAG, "Identified HVAC Unit %s OEM Scheme: %s",
               dev.address.to_string().c_str(), dev.oem_name.c_str());
    }
  } else if (opcode == 0x10D0) {
    auto dec =
        ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["filter_remaining_days"] =
          static_cast<float>(dec->remaining_days);
    }
  } else if (opcode == 0x22F1) {
    auto dec = ramses_esp::FanStatePayload::decode(msg.payload, msg.n_payload,
                                                   dev.hvac_scheme);
    if (dec.has_value()) {
      dev.last_telemetry["fan_speed"] = static_cast<float>(dec->speed_percent);
      ESP_LOGI(TAG, "HVAC Unit %s Fan Speed: %u%% (Preset: %s)",
               dev.address.to_string().c_str(), (unsigned)dec->speed_percent,
               ramses_esp::fan_preset_to_string(dec->preset_mode));
    }
  } else if (opcode == 0x1298) {
    auto dec = ramses_esp::Co2SensorPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["co2"] = static_cast<float>(dec->co2_ppm);
    }
  } else if (opcode == 0x12A0) {
    auto dec =
        ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      if (dec->temperature.has_value())
        dev.last_telemetry["indoor_temperature"] = *dec->temperature;
      if (dec->humidity.has_value())
        dev.last_telemetry["indoor_humidity"] = *dec->humidity;
    }
  }
}

void RamsesDiscoveryComponent::process_trv_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x3150) {
    auto dec =
        ramses_esp::HeatDemandPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["heat_demand"] = dec->demand_percent;
    }
  } else if (opcode == 0x1060) {
    auto dec =
        ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["battery_level"] = dec->battery_percent;
    }
  }
}

void RamsesDiscoveryComponent::process_opentherm_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x3220) {
    auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["modulation"] = dec->modulation_percent;
      if (dec->flow_temp.has_value())
        dev.last_telemetry["flow_temperature"] = *dec->flow_temp;
      if (dec->return_temp.has_value())
        dev.last_telemetry["return_temperature"] = *dec->return_temp;
    }
  }
}

void RamsesDiscoveryComponent::process_sensor_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x12C0) {
    auto dec = ramses_esp::OutdoorTemperaturePayload::decode(msg.payload,
                                                             msg.n_payload);
    if (dec.has_value() && dec->is_valid) {
      dev.last_telemetry["outdoor_temperature"] = dec->temperature;
    }
  }
}

void RamsesDiscoveryComponent::trigger_probe() {
  ESP_LOGI(
      TAG,
      "Manual probe triggered: resetting probe states and querying network");
  for (auto &kv : this->devices_) {
    kv.second.oem_probed = false;
    for (auto &zkv : kv.second.zones) {
      zkv.second.name_probed = false;
    }
  }
  this->probe_pending();
}

void RamsesDiscoveryComponent::probe_pending() {
#ifdef USE_ESP_IDF
  if (this->parent_ == nullptr)
    return;

  ramses_esp::RamsesAddress hgi_src{
      .dev_class = 18, .id = 0x005612, .is_valid = true};

  if (this->devices_.empty()) {
    ESP_LOGI(
        TAG,
        "No devices discovered yet; broadcasting discovery query (RQ 10E0)...");
    ramses_esp::RamsesAddress bcast{
        .dev_class = 63, .id = 0x3FFFFF, .is_valid = true};
    ramses_esp::RamsesMessage query =
        ramses_esp::DeviceInfoPayload::encode_query(hgi_src, bcast);
    this->parent_->send_message(query);
    return;
  }

  for (auto &kv : this->devices_) {
    DiscoveredDevice &dev = kv.second;

    // Probe un-named zones on controllers
    if (dev.device_type == "controller") {
      for (auto &zkv : dev.zones) {
        DiscoveredZone &zone = zkv.second;
        if (zone.name.empty() && !zone.name_probed) {
          zone.name_probed = true;
          ESP_LOGI(TAG, "Probing zone %d name on controller %s (RQ 0004)...",
                   (int)zone.index, dev.address.to_string().c_str());
          ramses_esp::RamsesMessage query =
              ramses_esp::ZoneNamePayload::encode_query(hgi_src, dev.address,
                                                        zone.index);
          this->parent_->send_message(query);
          return; // Send one probe per loop to avoid RF collisions
        }
      }
    }

    // Probe OEM signature and Status on HVAC units
    if (dev.is_hvac && !dev.oem_probed) {
      dev.oem_probed = true;
      ESP_LOGI(TAG, "Probing HVAC unit %s for Device Info (RQ 10E0)...",
               dev.address.to_string().c_str());
      ramses_esp::RamsesMessage query_info =
          ramses_esp::DeviceInfoPayload::encode_query(hgi_src, dev.address);
      this->parent_->send_message(query_info);

      ESP_LOGI(TAG, "Probing HVAC unit %s for Status (RQ 31DA)...",
               dev.address.to_string().c_str());
      ramses_esp::RamsesMessage query_status =
          ramses_esp::RamsesMessageBuilder::query()
              .from(hgi_src)
              .to(dev.address)
              .opcode(0x31DA)
              .payload({0x00})
              .build();
      this->parent_->send_message(query_status);
      return;
    }
  }
#endif
}

std::string RamsesDiscoveryComponent::generate_device_yaml(
    const DiscoveredDevice &dev) const {
  std::stringstream ss;
  std::string addr_str = dev.address.to_string();

  if (dev.device_type == "controller") {
    if (!dev.zones.empty()) {
      ss << "climate:\n";
      for (const auto &zkv : dev.zones) {
        const DiscoveredZone &z = zkv.second;
        std::string zone_name =
            z.name.empty() ? ("Zone " + std::to_string(z.index)) : z.name;
        ss << "  - platform: ramses_devices\n";
        ss << "    ramses_esp_id: ramses_hub\n";
        ss << "    name: \"" << zone_name << " Heating\"\n";
        ss << "    controller_address: \"" << addr_str << "\"\n";
        ss << "    zone_index: " << (int)z.index << "\n";
        ss << "    zone_name: \"" << zone_name << "\"\n\n";
      }
    }
    if (dev.has_dhw) {
      ss << "water_heater:\n";
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Domestic Hot Water\"\n";
      ss << "    controller_address: \"" << addr_str << "\"\n\n";
    }
  } else if (dev.is_hvac) {
    ss << "fan:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    name: \"Ventilation Unit\"\n";
    ss << "    device_address: \"" << addr_str << "\"\n";
    if (!dev.associated_remote.empty()) {
      ss << "    fake_remote_address: \"" << dev.associated_remote
         << "\"  # Clones your physical remote for zero-pairing control\n";
    }
    ss << "    scheme: " << dev.oem_name << "\n\n";

    ss << "sensor:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: filter_remaining_days\n";
    ss << "    name: \"Ventilation Filter Remaining Days\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "binary_sensor:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: filter_alarm\n";
    ss << "    name: \"Ventilation Filter Dirty Warning\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";
  } else if (dev.device_type == "remote") {
    ss << "# Discovered RF Remote Control: " << addr_str << "\n";
    if (!dev.associated_target.empty()) {
      ss << "# Paired to MVHR Unit: " << dev.associated_target << "\n";
      ss << "# (The fan configuration already clones this remote using "
            "fake_remote_address: \""
         << addr_str << "\")\n\n";
    }
  } else if (dev.device_type == "opentherm") {
    ss << "sensor:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: opentherm_modulation\n";
    ss << "    name: \"Boiler Modulation\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: flow_temperature\n";
    ss << "    name: \"Boiler Flow Temperature\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: return_temperature\n";
    ss << "    name: \"Boiler Return Temperature\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "binary_sensor:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: flame_active\n";
    ss << "    name: \"Boiler Flame Active\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: fault_alarm\n";
    ss << "    name: \"Boiler Fault Warning\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";
  } else if (dev.device_type == "trv") {
    ss << "sensor:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: heat_demand\n";
    ss << "    name: \"TRV " << addr_str << " Heat Demand\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: battery_level\n";
    ss << "    name: \"TRV " << addr_str << " Battery Level\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";

    ss << "binary_sensor:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    type: battery_low\n";
    ss << "    name: \"TRV " << addr_str << " Battery Low Warning\"\n";
    ss << "    ramses_address: \"" << addr_str << "\"\n\n";
  }

  return ss.str();
}

std::string RamsesDiscoveryComponent::generate_json(uint32_t now_ms) const {
  std::stringstream ss;
  ss << "{\n";
  ss << "  \"device_count\": " << this->devices_.size() << ",\n";
  ss << "  \"devices\": [\n";

  bool first_dev = true;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (!first_dev) {
      ss << ",\n";
    }
    first_dev = false;

    uint32_t last_seen_ago_sec = 0;
    if (now_ms > 0 && dev.last_seen_ms > 0 && now_ms >= dev.last_seen_ms) {
      last_seen_ago_sec = (now_ms - dev.last_seen_ms) / 1000;
    }

    std::string type_label = dev.device_type;
    if (dev.device_type == "controller") {
      type_label = "Evohome Controller";
    } else if (dev.device_type == "trv") {
      type_label = "Radiator TRV (HR92)";
    } else if (dev.device_type == "opentherm") {
      type_label = "OpenTherm Bridge (R8810)";
    } else if (dev.device_type == "remote") {
      type_label = "HVAC Remote Control";
      if (!dev.associated_target.empty()) {
        type_label += " (Paired to " + dev.associated_target + ")";
      }
    } else if (dev.is_hvac) {
      type_label = "MVHR Fan (" + dev.oem_name + ")";
    } else if (dev.device_type == "sensor") {
      type_label = "Room Sensor / Thermostat";
    } else if (dev.device_type == "gateway") {
      type_label = "HGI80 / RF Gateway";
    } else if (dev.device_type == "relay") {
      type_label = "BDR91 Relay";
    }

    ss << "    {\n";
    ss << "      \"address\": \"" << dev.address.to_string() << "\",\n";
    ss << "      \"device_type\": \"" << json_escape(dev.device_type)
       << "\",\n";
    ss << "      \"type_label\": \"" << json_escape(type_label) << "\",\n";
    ss << "      \"oem_name\": \"" << json_escape(dev.oem_name) << "\",\n";
    ss << "      \"associated_remote\": \""
       << json_escape(dev.associated_remote) << "\",\n";
    ss << "      \"associated_target\": \""
       << json_escape(dev.associated_target) << "\",\n";
    ss << "      \"rssi\": " << (int)dev.last_rssi << ",\n";
    ss << "      \"last_seen_sec\": " << last_seen_ago_sec << ",\n";
    ss << "      \"has_dhw\": " << (dev.has_dhw ? "true" : "false") << ",\n";

    // Zones
    ss << "      \"zones\": [";
    bool first_zone = true;
    for (const auto &zkv : dev.zones) {
      const DiscoveredZone &z = zkv.second;
      if (!first_zone)
        ss << ", ";
      first_zone = false;
      ss << "{\"index\": " << (int)z.index;
      ss << ", \"name\": \"" << json_escape(z.name) << "\"";
      if (z.has_temp)
        ss << ", \"temp\": " << std::fixed << std::setprecision(1)
           << z.last_temp;
      if (z.has_setpoint)
        ss << ", \"setpoint\": " << std::fixed << std::setprecision(1)
           << z.last_setpoint;
      ss << "}";
    }
    ss << "],\n";

    // Telemetry
    ss << "      \"telemetry\": {";
    bool first_tel = true;
    for (const auto &tkv : dev.last_telemetry) {
      if (!first_tel)
        ss << ", ";
      first_tel = false;
      ss << "\"" << json_escape(tkv.first) << "\": " << std::fixed
         << std::setprecision(1) << tkv.second;
    }
    ss << "},\n";

    // Per-device YAML
    std::string dev_yaml = this->generate_device_yaml(dev);
    ss << "      \"yaml\": \"" << json_escape(dev_yaml) << "\"\n";
    ss << "    }";
  }

  ss << "\n  ],\n";
  ss << "  \"full_yaml\": \"" << json_escape(this->generate_yaml()) << "\"\n";
  ss << "}\n";

  return ss.str();
}

std::string RamsesDiscoveryComponent::generate_yaml() const {
  std::stringstream ss;

  ss << "# ==========================================================\n";
  ss << "# Automatically Generated RAMSES Configuration\n";
  ss << "# Generated by ramses_discovery\n";
  ss << "# ==========================================================\n\n";

  // 1. Climate Platform
  bool has_climate = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.device_type == "controller" && !dev.zones.empty()) {
      if (!has_climate) {
        ss << "climate:\n";
        has_climate = true;
      }
      for (const auto &zkv : dev.zones) {
        const DiscoveredZone &z = zkv.second;
        std::string zone_name =
            z.name.empty() ? ("Zone " + std::to_string(z.index)) : z.name;
        ss << "  - platform: ramses_devices\n";
        ss << "    ramses_esp_id: ramses_hub\n";
        ss << "    name: \"" << zone_name << " Heating\"\n";
        ss << "    controller_address: \"" << dev.address.to_string() << "\"\n";
        ss << "    zone_index: " << (int)z.index << "\n";
        ss << "    zone_name: \"" << zone_name << "\"\n\n";
      }
    }
  }

  // 2. Water Heater Platform
  bool has_dhw = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.has_dhw) {
      if (!has_dhw) {
        ss << "water_heater:\n";
        has_dhw = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Domestic Hot Water\"\n";
      ss << "    controller_address: \"" << dev.address.to_string() << "\"\n\n";
    }
  }

  // 3. Fan Platform
  bool has_fan = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.is_hvac) {
      if (!has_fan) {
        ss << "fan:\n";
        has_fan = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Ventilation Unit\"\n";
      ss << "    device_address: \"" << dev.address.to_string() << "\"\n";
      if (!dev.associated_remote.empty()) {
        ss << "    fake_remote_address: \"" << dev.associated_remote
           << "\"  # Clones your physical remote for zero-pairing control\n";
      }
      ss << "    scheme: " << dev.oem_name << "\n\n";
    }
  }

  // 4. Sensor Platform
  bool has_sensor = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();

    if (dev.device_type == "trv") {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: heat_demand\n";
      ss << "    name: \"TRV " << addr_str << " Heat Demand\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: battery_level\n";
      ss << "    name: \"TRV " << addr_str << " Battery Level\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.device_type == "opentherm") {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: opentherm_modulation\n";
      ss << "    name: \"Boiler Modulation\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: flow_temperature\n";
      ss << "    name: \"Boiler Flow Temperature\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: return_temperature\n";
      ss << "    name: \"Boiler Return Temperature\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.is_hvac) {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: filter_remaining_days\n";
      ss << "    name: \"Ventilation Filter Remaining Days\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    }
  }

  // 5. Binary Sensor Platform
  bool has_bin = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();

    if (dev.device_type == "trv") {
      if (!has_bin) {
        ss << "binary_sensor:\n";
        has_bin = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: battery_low\n";
      ss << "    name: \"TRV " << addr_str << " Battery Low Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.device_type == "opentherm") {
      if (!has_bin) {
        ss << "binary_sensor:\n";
        has_bin = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: flame_active\n";
      ss << "    name: \"Boiler Flame Active\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: fault_alarm\n";
      ss << "    name: \"Boiler Fault Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.is_hvac) {
      if (!has_bin) {
        ss << "binary_sensor:\n";
        has_bin = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: filter_alarm\n";
      ss << "    name: \"Ventilation Filter Dirty Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    }
  }

  return ss.str();
}

void RamsesDiscoveryComponent::dump_yaml() const {
  std::string yaml = this->generate_yaml();
  ESP_LOGI(TAG, "\n%s", yaml.c_str());
}

} // namespace ramses_discovery
} // namespace esphome

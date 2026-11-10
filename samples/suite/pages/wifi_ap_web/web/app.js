(() => {
  "use strict";

  const copy = {
    ko: {
      title: "기기 스위치", intro: "이 휴대폰과 HMI의 상태가 함께 바뀝니다.",
      switch1: "스위치 1", switch2: "스위치 2", switch3: "스위치 3", switch4: "스위치 4",
      on: "켜짐", off: "꺼짐", local: "로컬 연결 전용",
      connecting: "연결 확인 중", connectingDetail: "HMI에 연결하고 있습니다.",
      connected: "HMI 연결됨", connectedDetail: "변경 사항이 양쪽 화면에 바로 반영됩니다.",
      reconnecting: "재연결 대기", reconnectingDetail: "HMI 연결을 다시 시도하고 있습니다.",
      denied: "접근 정보 확인 필요", deniedDetail: "HMI 화면의 QR을 다시 스캔하세요."
    },
    en: {
      title: "Device switches", intro: "This phone and the HMI always show the same state.",
      switch1: "Switch 1", switch2: "Switch 2", switch3: "Switch 3", switch4: "Switch 4",
      on: "On", off: "Off", local: "Local connection only",
      connecting: "Checking connection", connectingDetail: "Connecting to the HMI.",
      connected: "HMI connected", connectedDetail: "Changes appear on both screens immediately.",
      reconnecting: "Waiting to reconnect", reconnectingDetail: "Trying the HMI connection again.",
      denied: "Connection details required", deniedDetail: "Scan the QR code on the HMI again."
    }
  };
  const language = navigator.language && navigator.language.toLowerCase().startsWith("ko") ? "ko" : "en";
  const words = copy[language];
  const controls = Array.from(document.querySelectorAll(".control"));
  const connection = document.querySelector(".connection");
  const title = document.getElementById("connection-title");
  const detail = document.getElementById("connection-detail");
  const revisionLabel = document.getElementById("revision");
  const token = new URLSearchParams(location.hash.slice(1)).get("token") || "";
  let socket = null;
  let revision = 0;
  let switches = [false, false, false, false];
  let reconnectTimer = 0;
  let authenticated = false;
  let reconnectAttempt = 0;
  let blocked = false;
  const reconnectDelays = [500, 1000, 2000, 5000];

  document.documentElement.lang = language;
  document.querySelectorAll("[data-copy]").forEach((node) => {
    node.textContent = words[node.dataset.copy];
  });

  function setConnection(state, heading, message) {
    connection.dataset.state = state;
    title.textContent = heading;
    detail.textContent = message;
    controls.forEach((button) => { button.disabled = state !== "connected"; });
  }

  function render(snapshot) {
    revision = snapshot.revision >>> 0;
    switches = snapshot.switches.map(Boolean).slice(0, 4);
    while (switches.length < 4) switches.push(false);
    controls.forEach((button, index) => {
      const value = switches[index];
      button.setAttribute("aria-pressed", String(value));
      button.querySelector(".control-value").textContent = value ? words.on : words.off;
    });
    revisionLabel.textContent = `Revision ${revision}`;
  }

  function connect() {
    clearTimeout(reconnectTimer);
    blocked = false;
    setConnection("connecting", words.connecting, words.connectingDetail);
    const scheme = location.protocol === "https:" ? "wss:" : "ws:";
    socket = new WebSocket(scheme + String.fromCharCode(47, 47) + location.host + "/ws");
    socket.addEventListener("open", () => socket.send(JSON.stringify({ token })));
    socket.addEventListener("message", (event) => {
      let message;
      try { message = JSON.parse(event.data); } catch (_) { return; }
      if (message.type === "denied") {
        authenticated = false;
        blocked = true;
        setConnection("error", words.denied, words.deniedDetail);
        socket.close();
        return;
      }
      if (message.type === "snapshot" && Array.isArray(message.switches)) {
        authenticated = true;
        reconnectAttempt = 0;
        render(message);
        setConnection("connected", words.connected, words.connectedDetail);
      }
    });
    socket.addEventListener("close", () => {
      if (blocked || (!authenticated && token.length !== 32)) {
        setConnection("error", words.denied, words.deniedDetail);
        return;
      }
      authenticated = false;
      setConnection("reconnecting", words.reconnecting, words.reconnectingDetail);
      const delay = reconnectDelays[Math.min(reconnectAttempt,
                                             reconnectDelays.length - 1)];
      reconnectAttempt += 1;
      reconnectTimer = setTimeout(connect, delay);
    });
    socket.addEventListener("error", () => socket.close());
  }

  controls.forEach((button) => button.addEventListener("click", () => {
    if (!socket || socket.readyState !== WebSocket.OPEN || !authenticated) return;
    const id = Number(button.dataset.switch);
    socket.send(JSON.stringify({ id, value: !switches[id - 1], revision }));
    button.disabled = true;
  }));

  connect();
})();

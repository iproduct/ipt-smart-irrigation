export interface SensorData {
  temperature: number;
  humidity: number;
  soilMoisture: number;
}

export interface IoTData {
  type: "state";
  time: number;
  start_time: number;
  deviceId: string;
  valves: number[];
  flows: number[];
  moists: number[];
  volumes: number[];
}

export interface OpenValveCommand {
  deviceId: string;
  command: "OPEN_VALVE";
  valve: number;
}

export interface CloseValveCommand {
  deviceId: string;
  command: "CLOSE_VALVE";
  valve: number;
}

export interface OpenValvesCommand {
  deviceId: string;
  command: "OPEN_VALVES";
  valves: number[];
}

export interface CommandAckMessage {
  type: "command_ack";
  deviceId: string;
  command: string; // The command that was acknowledged, as a JSON string
}

export interface ErrorMessage {
  type: "error";
  message: string;
}

export interface Zone {
  id: string;
  name: string;
  wateringRequirementLiters: number;
  wateringIntervalHours: number;
  valveNumber: number;
  flowmeter: number;
  moistureSensors: number[];
  displayPosition: number;
}

export type WebSocketCommand = OpenValveCommand | OpenValvesCommand | CloseValveCommand;

export type WebSocketIncomingMessage = IoTData | CommandAckMessage | ErrorMessage;

const WS_URL = 'ws://192.168.0.17:8080/ws';

let ws: WebSocket | null = null;
let reconnectTimeoutId: ReturnType<typeof setTimeout> | null = null; // New: to store timeout ID

export const connectWebSocket = (onMessage: (data: WebSocketIncomingMessage) => void, onError: (error: Event | ErrorMessage) => void, onOpen: () => void) => {
  // If a connection is already open or in the process of opening, do nothing.
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
    return;
  }

  // Clear any pending reconnection attempts
  if (reconnectTimeoutId) {
    clearTimeout(reconnectTimeoutId);
    reconnectTimeoutId = null;
  }

  ws = new WebSocket(WS_URL);

  ws.onopen = () => {
    console.log('WebSocket connected');
    onOpen();
  };

  ws.onmessage = (event) => {
    try {
      const parsedData = JSON.parse(event.data);
      if (parsedData.type === "state") {
        const iotDataWithVolume: IoTData = {
          ...parsedData,
          time: parsedData.time + parsedData.start_time,
          flows: parsedData.flows || [],
          moists: parsedData.moists || [],
          volumes: (parsedData.flows || []).map((flow: number) => flow / 396.0),
        };
        onMessage(iotDataWithVolume as IoTData);
      } else if (parsedData.type === "command_ack") {
        onMessage(parsedData as CommandAckMessage);
      } else if (parsedData.type === "error") {
        try {
          const errorMessage = JSON.parse(parsedData.message);
          if (errorMessage.error) {
            onError({ type: "error", message: errorMessage.error });
          } else {
            console.warn("Unknown error message format:", parsedData.message);
          }
        } catch (parseError) {
          console.error("Error parsing error message:", parseError);
          onError({ type: "error", message: parsedData.message });
        }
      } else {
        console.warn("Unknown WebSocket message type:", parsedData.type);
      }
    } catch (error) {
      console.error('Error parsing WebSocket message:', error);
    }
  };

  ws.onclose = (event) => {
    console.log('WebSocket disconnected', event.code, event.reason);
    // Only attempt to reconnect if the closure was not intentional (e.g., ws.close() was called cleanly)
    if (!event.wasClean) {
      reconnectTimeoutId = setTimeout(() => connectWebSocket(onMessage, onError, onOpen), 5000);
    }
  };

  ws.onerror = (error) => {
    console.error('WebSocket error:', error);
    onError(error);
    // Close the connection on error to trigger onclose, which handles reconnect logic
    if (ws && ws.readyState !== WebSocket.CLOSED) {
      ws.close();
    }
  };
};

export const disconnectWebSocket = () => {
  if (ws) {
    // Attempt to gracefully close if open or connecting
    if (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING) {
      ws.close(1000, "Client initiated disconnect"); // Use a standard close code
    }
    ws = null;
  }
  // Clear any pending reconnection timeout as well
  if (reconnectTimeoutId) {
    clearTimeout(reconnectTimeoutId);
    reconnectTimeoutId = null;
  }
};

export const sendWebSocketMessage = (message: WebSocketCommand) => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(message));
  } else {
    console.warn('WebSocket is not open. Message not sent:', message);
  }
};

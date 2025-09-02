# Smart Irrigation Dashboard (React Client)

## Project Description

This project is the React client for a Smart Irrigation system. It provides a dashboard for monitoring sensor data (flow, moisture, temperature, humidity), controlling irrigation valves, and managing irrigation zones. The client communicates with a backend API via WebSockets for real-time data and REST API for zone management.

## React Client Overview

The client application is built using React with TypeScript and utilizes the Material-UI component library for a modern and responsive user interface. It features:

*   **Real-time Sensor Data**: Displays live data from flowmeters and moisture sensors, including calculated water volume.
*   **Irrigation Control**: Allows individual control of irrigation valves through a WebSocket connection.
*   **Zone Management**: A dedicated page for CRUD (Create, Read, Update, Delete) operations on irrigation zones, interacting with a backend REST API.
*   **Historical Data Visualization**: Uses Echarts to display historical flow volume and moisture sensor data in separate, resizable charts.
*   **Responsive Design**: The dashboard is designed to adapt to various screen sizes.

## Setup and Installation

To set up and run the React client application locally, follow these steps:

### Prerequisites

*   Node.js (LTS version recommended)
*   npm (Node Package Manager)
*   A running backend API for zone management and WebSocket server for sensor data (e.g., at `http://192.168.0.17:8080`).

### Installation Steps

1.  **Clone the repository (if you haven't already):**
    ```bash
    git clone <your-repository-url>
    cd smart-irrigation-client
    ```

2.  **Install dependencies:**
    Navigate to the `smart-irrigation-client` directory and install the required Node.js packages:
    ```bash
    npm install
    ```

## Running the Application

To start the development server and run the React client:

```bash
npm run dev
```

This will usually open the application in your default web browser at `http://localhost:5173` (or another available port).

## Important Notes

*   **Backend API**: This client relies on a separate backend API for full functionality. Ensure your backend is running and accessible at the configured endpoints (`http://192.168.0.17:8080/api/zones` for REST and `ws://192.168.0.17:8080/ws` for WebSockets).
*   **Zone Configuration**: The dashboard's flowmeter and moisture sensor visualizations are tied to the `valveNumber`, `flowmeter`, and `moistureSensors` properties configured in the Zone Management page. Ensure these are correctly set up in your zones for accurate display.

## Technologies Used

*   React
*   TypeScript
*   Material-UI
*   Echarts & echarts-for-react
*   React Router
*   WebSockets (for real-time data)

## Further Development

Here are some ideas for extending this project:

*   **User Authentication**: Implement login/logout functionality and secure API endpoints.
*   **Enhanced UI/UX**: Further refine the visual design, add animations, and improve user interaction.
*   **Notifications**: Implement real-time notifications for critical sensor readings or system events.
*   **Advanced Scheduling**: Develop a more sophisticated irrigation scheduling system with calendar integration.
*   **Sensor Calibration**: Add functionality to calibrate sensors directly from the dashboard.
*   **Mobile Responsiveness**: Optimize the layout and components specifically for mobile devices.
*   **Backend Integration**: Connect to actual hardware for sensor readings and valve control.

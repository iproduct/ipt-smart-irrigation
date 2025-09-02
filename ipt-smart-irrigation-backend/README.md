# Smart Irrigation Backend

This project implements the backend for a smart irrigation system using Spring Boot, Spring WebFlux, Apache Kafka, and CoAP. It is designed to communicate with IoT irrigation controllers, process sensor data, and manage irrigation zones reactively.

## Features

*   **Reactive Web Services**: RESTful APIs for managing irrigation zones.
*   **CoAP Server**: Communication with IoT irrigation controllers for sensor data and commands.
*   **Apache Kafka Integration**: Real-time processing of sensor data using Kafka Streams and publishing of commands.
*   **Reactive MongoDB Persistence**: Storage and retrieval of irrigation zone configurations.
*   **WebSockets**: Real-time sensor data visualization in the frontend.

## Backend Architecture

The application is built around a reactive and event-driven architecture.

### 1. Spring Boot Application (`KafkaStreamsRobotDemoApplication`)
This is the main entry point of the application, configuring and starting the various servers and services.

### 2. Spring WebFlux Server
Provides reactive RESTful endpoints and serves the frontend application.
*   **`ZoneHandler`**: Handles HTTP requests for CRUD operations on `Zone` entities (e.g., finding zones by ID or name, creating, updating, deleting zones, and getting the total count).
*   **`ZoneRouter`**: Defines the functional routes for the `/api/zones` endpoint, mapping HTTP methods (GET, POST, PUT, DELETE) to the corresponding `ZoneHandler` methods.
*   **`WebConfig`**: Configures WebFlux, including CORS settings, and registers the `ZoneRouter` to make the zone management endpoints accessible. It also configures a WebSocket endpoint.

### 3. CoAP Server
Facilitates communication with CoAP-enabled IoT irrigation controllers.
*   **`HelloResource`**: A simple endpoint for testing CoAP connectivity.
*   **`TimeResource`**: Provides the current server time to CoAP clients.
*   **`RegisterClientResource`**: Allows IoT controllers to register themselves with the backend, providing their IP and port for future command delivery.
*   **`SensorsResource`**: Receives sensor data (e.g., flow, moisture) from irrigation controllers via PUT requests. This data is then processed and published to Kafka.

### 4. Apache Kafka Integration

*   **Kafka Publisher**: Sensor data received via CoAP is published to a Kafka topic (e.g., `sweepDistances`).
*   **Kafka Streams (`KafkaStreamsConfig`)**: Processes incoming sensor data streams. For example, it can aggregate sensor readings or detect specific events. The `kStream` bean defines a stream that consumes from "sweepDistances" and produces to "minSweepDistance" after some processing.

### 5. Reactive MongoDB Persistence
*   **`spring-boot-starter-data-mongodb-reactive`**: Enables non-blocking, reactive data access to MongoDB.
*   **`Zone` Model**: Represents an irrigation zone with properties like `id`, `name`, `wateringRequirementLiters`, `wateringIntervalHours`, `valveNumber`, `flowmeter`, and `moistureSensors`.
*   **`ZoneRepository`**: A reactive repository interface extending `ReactiveMongoRepository` for `Zone` entities, providing basic CRUD and custom query methods (e.g., `findByName`).

### 6. ReactiveRobotService
Manages the flow of sensor readings and commands within the application.
*   It exposes `Sinks.Many<String>` for `sensorReadings` and `commands`, allowing different components to emit and subscribe to these reactive streams.
*   It acts as a bridge between the CoAP server (receiving sensor data, sending commands) and the WebSocket handler (streaming sensor data to clients).

### 7. Connections Between Major Components

*   **IoT Controller <-> CoAP Server**: CoAP is used for lightweight communication, allowing controllers to send sensor data and receive commands.
*   **CoAP Server -> Kafka**: Sensor data received by the CoAP server is immediately published to Kafka for real-time processing.
*   **Kafka Streams -> ReactiveRobotService**: Processed data from Kafka Streams can be consumed by `ReactiveRobotService` or other services.
*   **ReactiveRobotService <-> WebFlux WebSocket**: Real-time sensor data is streamed from `ReactiveRobotService` to connected WebSocket clients (e.g., the frontend dashboard).
*   **WebFlux REST -> ZoneService -> ZoneRepository -> MongoDB**: The WebFlux REST endpoints for zones interact with the `ZoneService`, which in turn uses the `ZoneRepository` to persist and retrieve `Zone` data from MongoDB.
*   **WebFlux Frontend <-> WebFlux REST / WebSocket**: The frontend interacts with the WebFlux REST APIs for configuration and with the WebSocket endpoint for real-time data.

## Data Models

*   **`Zone`**: Represents an irrigation zone, including its watering requirements, associated valve, flowmeter, and moisture sensors.
*   **`IrrigationControllerState`**: Captures the current state of an irrigation controller, including its device ID, valve states, flow readings, and moisture sensor values.

## Prerequisites

Before running the application, ensure you have the following installed:

*   **Java Development Kit (JDK) 18 or higher**
*   **Gradle**: Build automation tool (typically included in project wrapper `gradlew`).
*   **Apache Kafka**: A distributed streaming platform.
*   **MongoDB**: A NoSQL document database.

## How to Run

1.  **Start MongoDB**:
    Ensure your MongoDB instance is running, typically on `localhost:27017`. The database `smart_irrigation` will be created automatically upon first use.

2.  **Start Apache Kafka**:
    Start your Kafka broker (and Zookeeper, if managed separately). The application expects Kafka to be running on `localhost:9092` (configured in `application.properties`).

3.  **Build the Application**:
    Navigate to the project root directory in your terminal and run:
    ```bash
    ./gradlew clean build
    ```

4.  **Run the Application**:
    After a successful build, you can run the application:
    ```bash
    java -jar build/libs/kafka-streams-robot-demo-0.0.1-SNAPSHOT.jar
    ```
    (Note: The exact JAR filename might vary slightly based on your Gradle configuration.)

5.  **Access WebFlux Endpoints and UI**:
    *   The application will be accessible on `http://localhost:8080` (or the address configured in `application.properties`).
    *   The frontend UI (if available) will be served from the root path `/`.
    *   Zone management API endpoints are available under `/api/zones` (e.g., `GET /api/zones`, `POST /api/zones`).

6.  **Interacting with CoAP Devices**:
    CoAP clients can interact with the server on `coap://192.168.0.17:5683` (or the IP/port configured in `KafkaStreamsRobotDemoApplication`).

## Future Enhancements

*   Implement authentication and authorization for WebFlux endpoints.
*   Add more sophisticated Kafka Streams topologies for advanced sensor data analytics.
*   Develop a more comprehensive frontend user interface for managing zones and visualizing data.
*   Implement command and control logic for irrigation valves based on sensor data.

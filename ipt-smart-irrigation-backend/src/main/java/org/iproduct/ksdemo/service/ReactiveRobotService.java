package org.iproduct.ksdemo.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import reactor.core.publisher.Sinks;

import java.time.Duration;

@Service
public class ReactiveRobotService {
    @Value("${events.replay.maxage}")
    private int replayEventsMaxAge = 60000;
    @Value("${events.replay.historysize}")
    private int replayHistorySize = 500;

    private Sinks.Many<String> sensorReadings = Sinks.unsafe().many().replay().limit(replayHistorySize, Duration.ofMillis(replayEventsMaxAge));
    private Sinks.Many<String> commands = Sinks.unsafe().many().multicast().directBestEffort();
    public Sinks.Many<String> getSensorReadings() {
        return sensorReadings;
    }
    public Sinks.Many<String> getCommands() {
        return commands;
    }
}

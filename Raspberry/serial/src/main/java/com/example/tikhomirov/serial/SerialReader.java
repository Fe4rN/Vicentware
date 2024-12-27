package com.example.tikhomirov.serial;

import com.pi4j.io.serial.Serial;
import com.pi4j.util.Console;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Date;
import java.util.concurrent.ScheduledExecutorService;
import java.lang.String;

public class SerialReader implements Runnable, MqttCallback {
    private final Console console;
    private final Serial serial;
    private boolean continueReading = true;

    private static final String topic = "VicentPI/edificio1";
    private static final int qos = 1;
    private static final boolean retain = false;
    private static final String broker = "tcp://broker.mqtt.cool.1883";
    private static final String clientId = "vecino1";
    private MqttClient client;
    private ScheduledExecutorService scheduler;

    public SerialReader(Console console, Serial serial) {
        this.console = console;
        this.serial = serial;
    }

    public void stopReading() {
        continueReading = false;
    }

    @Override
    public void run(){
        BufferedReader br = new BufferedReader(new InputStreamReader(serial.getInputStream()));
        try {
            String line = "";

            client = new MqttClient(broker, clientId, new MemoryPersistence()); // Conexión con el bróker
            MqttConnectOptions connOpts = new MqttConnectOptions();
            connOpts.setCleanSession(true);
            connOpts.setKeepAliveInterval(60);
            connOpts.setWill(topic, "Desconectada!".getBytes(), qos, retain);
            client.connect(connOpts);
            client.setCallback(this); // Callback

            // Suscripción al tópico
            client.subscribe(topic, qos);
            console.println("MQTT", "Suscripción al tópico: " + topic);

            while (continueReading) {
                if (serial.available() > 0) {
                    int data = br.read(); // Read a single byte
                    if (data == -1) {
                        console.println("Stream ended. Attempting to recover...");
                        recoverSerialConnection();
                        continue;
                    }

                    char c = (char) data;
                    if (c == '\n' || c == '\r') { // End of line
                        if (!line.isEmpty()) {
                            processLine(line);
                            line = "";
                        }
                    } else {
                        line += c;
                    }
                } else {
                    Thread.sleep(10); // Allow CPU to rest
                }
            }
        } catch (Exception e) {
            console.println("Error al leer Serial: " + e.getMessage());
        }
    }

    private void processLine(String line) {
        try {
            // Validate packet format
            if (!line.startsWith("<") || !line.endsWith(">")) {
                console.println("Invalid packet format: " + line);
                return;
            }

            // Remove start and end markers
            line = line.substring(1, line.length() - 1);

            // Split and parse the fields
            String[] parts = line.split(",");

            if (parts.length != 6) {
                console.println("Invalid data format: " + line);
            }

            // Parse data
            boolean motionDetected = parts[0].equals("0");
            String cardUID = parts[1];
            double luminosity = Double.parseDouble(parts[2].isEmpty() ? "0" : parts[2]); // Default to 0 if empty
            double gasLevel = Double.parseDouble(parts[3].isEmpty() ? "0" : parts[3]); // Default to 0 if empty
            boolean noiseDetected = parts[4].equals("1");

            JSONObject json = new JSONObject();
            json.put("fecha", new Date());
            json.put("temperatura", 19.3);
            json.put("distancia", motionDetected ? true : false);
            json.put("gas", gasLevel + "%");
            json.put("luz", luminosity + "%");
            json.put("ruido", noiseDetected? true : false);

            MqttMessage message = new MqttMessage(json.toString().getBytes());
            message.setQos(qos);
            message.setRetained(retain);
            client.publish(topic, message);
            console.println("MQTT", "Mensaje enviado: " + json.toString());

            // Print parsed data
            console.println("Motion Detected: " + motionDetected);
            console.println("Card UID: " + cardUID);
            console.println("Luminosity: " + luminosity + " %");
            console.println("Gas Level: " + gasLevel + " %");
            console.println("Noise Detected: " + noiseDetected);
        } catch (Exception e) {
            console.println("Error processing line: " + e.getMessage());
        }


    }


    private void recoverSerialConnection() {
        try {
            console.println("Attempting to reset serial connection...");
            serial.close();
            Thread.sleep(1000); // Allow time for the device to reset
            serial.open(); // Update with your serial port and baud rate
            console.println("Serial connection restored.");
        } catch (Exception e) {
            console.println("Failed to recover serial connection: " + e.getMessage());
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        console.println("Conexion perdida");
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) throws Exception {

    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {
        console.println("MQTT", "Entrega completa!");
    }
}

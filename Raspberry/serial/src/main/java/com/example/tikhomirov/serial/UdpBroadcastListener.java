package com.example.tikhomirov.serial;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.Date;

import com.pi4j.Pi4J;
import com.pi4j.context.Context;
import com.pi4j.io.exception.IOException;
import com.pi4j.util.Console;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

import org.json.JSONObject;

public class UdpBroadcastListener implements MqttCallback {

    private final Console console;
    private final Context pi4j;
    private MqttClient client;

    private final String topic = "VicentPI/edificio1";
    private final String nfcTopic = "VicentPI/accesos";
    private final int qos = 1;
    private final boolean retain = false;
    private final String broker = "tcp://broker.hivemq.com:1883";
    private final String clientId = "vecino1";

    public UdpBroadcastListener() throws Exception {
        console = new Console();
        pi4j = Pi4J.newAutoContext();

        // Initialize MQTT client
        client = new MqttClient(broker, clientId, new MemoryPersistence());
        MqttConnectOptions connOpts = new MqttConnectOptions();
        connOpts.setCleanSession(true);
        connOpts.setKeepAliveInterval(60);
        connOpts.setWill(topic, "Desconectada!".getBytes(), qos, retain);

        client.setCallback(this); // Set MQTT callback
        client.connect(connOpts);

        console.println("Connected to MQTT broker.");

        client.subscribe(nfcTopic, qos);
        console.println("Subscribed to NFC topic: " + nfcTopic);
    }

    public void startListening() {
        int port = 1234;
        byte[] receiveBuffer = new byte[1024];

        try (DatagramSocket socket = new DatagramSocket(port)) {
            console.println("Listening for UDP broadcasts on port " + port + "...");

            while (true) {
                DatagramPacket packet = new DatagramPacket(receiveBuffer, receiveBuffer.length);
                socket.receive(packet);

                String receivedData = new String(packet.getData(), 0, packet.getLength()).trim();
                InetAddress senderAddress = packet.getAddress();

                console.println("Datos recibidos.");

                if (receivedData.matches("<\\d+,\\d+(\\.\\d+)?,\\d+(\\.\\d+)?,\\d+>")) {
                    String[] values = receivedData.substring(1, receivedData.length() - 1).split(",");

                    try {
                        int trigProx = Integer.parseInt(values[0]); // Integer
                        float valLum = Float.parseFloat(values[1]); // Float
                        float valGas = Float.parseFloat(values[2]); // Float
                        int valRui = Integer.parseInt(values[3]); // Integer

                        console.println("Valores:");
                        console.println("trigProx: " + trigProx);
                        console.println("valLum: " + valLum);
                        console.println("valGas: " + valGas);
                        console.println("valRui: " + valRui);

                        if (trigProx == 1) {
                            // Trigger camera capture
                            takePicture();
                        }

                        // Create the JSON object for MQTT payload
                        JSONObject json = new JSONObject();
                        json.put("fecha", new Date()); // Current timestamp
                        json.put("temperatura", 19.3); // Placeholder temperature (adjust as needed)
                        json.put("distancia", trigProx == 1); // Convert trigProx to boolean
                        json.put("gas", String.format("%.2f%%", valGas)); // Gas level as percentage
                        json.put("luz", String.format("%.2f%%", valLum)); // Luminosity as percentage
                        json.put("ruido", valRui == 1); // Convert valRui to boolean

                        // Publish JSON object to MQTT
                        MqttMessage message = new MqttMessage(json.toString().getBytes());
                        message.setQos(qos);
                        message.setRetained(retain);
                        client.publish(topic, message);
                    } catch (NumberFormatException e) {
                        console.println("Error al transformar valores: " + e.getMessage());
                    }
                } else {
                    console.println("Formato de datos invalido: " + receivedData);
                }
            }
        } catch (Exception e) {
            System.err.println("Error while listening for UDP broadcasts: " + e.getMessage());
        } finally {
            pi4j.shutdown();
        }
    }

    private void takePicture() {
        try {
            // Generate a unique filename using the current timestamp
            String filename = "/home/pi/pictures/picture_" + System.currentTimeMillis() + ".jpg";

            // Run the shell command to capture the image
            Process process = Runtime.getRuntime().exec("raspistill -o " + filename);
            process.waitFor();

            console.println("Picture taken and saved as: " + filename);
        } catch (IOException | InterruptedException e) {
            console.println("Error taking picture: " + e.getMessage());
        } catch (java.io.IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void connectionLost(Throwable throwable) {
        console.println("Conexion con MQTT perdida: " + throwable.getMessage());
    }

    @Override
    public void messageArrived(String s, MqttMessage mqttMessage) throws Exception {
        console.println("Message arrived from topic: " + s + " - " + mqttMessage.toString());

        if (topic.equals(nfcTopic)) {
            String nfcUid = mqttMessage.toString();
            console.println("Received NFC UID: " + nfcUid);
        }
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken iMqttDeliveryToken) {
        console.println("Mensaje enviado con exito.");
    }

    public static void main(String[] args) {
        try {
            UdpBroadcastListener listener = new UdpBroadcastListener();
            listener.startListening();
        } catch (Exception e) {
            System.err.println("Error starting listener: " + e.getMessage());
        }
    }
}

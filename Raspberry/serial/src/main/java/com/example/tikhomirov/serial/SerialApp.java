package com.example.tikhomirov.serial;

import com.pi4j.context.Context;
import com.pi4j.Pi4J;
import com.pi4j.io.gpio.digital.*;
import com.pi4j.io.serial.FlowControl;
import com.pi4j.io.serial.Parity;
import com.pi4j.io.serial.Serial;
import com.pi4j.io.serial.StopBits;
import com.pi4j.util.Console;

public class SerialApp {

    public static void main(String[] args) throws Exception {
        Console console = new Console();
        Context pi4j = Pi4J.newAutoContext();

        Serial serial = pi4j.create(Serial.newConfigBuilder(pi4j)
                .use_115200_N81()
                .dataBits_8()
                .parity(Parity.NONE)
                .stopBits(StopBits._1)
                .flowControl(FlowControl.NONE)
                .id("serial_vincent")
                .device("/dev/serial0")
                .provider("pigpio-serial")
                .build());
        serial.open();

        console.print("Esperando a que se abra el puerto Serial");
        while(!serial.isOpen()) {
            Thread.sleep(250);
        }

        SerialReader serialReader = new SerialReader(console, serial);
        Thread serialReaderThread = new Thread(serialReader, "SerialReader");
        serialReaderThread.setDaemon(true);
        serialReaderThread.start();

        while (serial.isOpen()) {
            Thread.sleep(500);
        }
        serialReader.stopReading();
    }
}
package com.example.tikhomirov.serial;

import java.io.IOException;
import java.time.Duration;
import java.util.concurrent.TimeUnit;

public class Camera {

    public static class PictureConfig {
        public String outputPath = "/home/pi/Pictures/";
        public String fileName = "image.jpg";
        public int width = 1280;
        public int height = 800;
        public boolean useDate = false;
        public boolean disablePreview = false;
        public int quality = 90;
    }

    public void takePicture(PictureConfig config) {
        String dateOption = config.useDate ? "-t 1" : "";
        String previewOption = config.disablePreview ? "--nopreview" : "";

        String command = String.format(
                "libcamera-still -o %s%s --width %d --height %d %s %s -q %d",
                config.outputPath,
                config.fileName,
                config.width,
                config.height,
                dateOption,
                previewOption,
                config.quality
        );

        try {
            Process process = Runtime.getRuntime().exec(command);
            process.waitFor(5, TimeUnit.SECONDS); // Wait for 5 seconds to allow picture capture.
            if (process.exitValue() != 0) {
                System.err.println("Error taking picture");
            } else {
                System.out.println("Picture taken successfully!");
            }
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
        }
    }

    public static PictureConfig newPictureConfigBuilder() {
        return new PictureConfig();
    }

    public void reset() {
        System.out.println("Camera reset is not implemented yet.");
    }
}

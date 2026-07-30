# install_deps.ps1
# This script installs all required Arduino libraries for DigiFrame using arduino-cli.

Write-Host "Installing Arduino dependencies for DigiFrame..."

# List of libraries to install (exact names from Library Manager)
$libraries = @(
    "ArduinoJson@7.4.3",
    "ESP32 HUB75 LED MATRIX PANEL DMA Display@3.0.14",
    "Adafruit GFX Library@1.12.6",
    "Adafruit BusIO@1.17.4",
    "AnimatedGIF@2.2.0",
    "PubSubClient@2.8"
)

foreach ($lib in $libraries) {
    Write-Host "Installing $lib..."
    arduino-cli lib install "$lib"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error installing $lib" -ForegroundColor Red
    }
}

Write-Host "Done!" -ForegroundColor Green

#include "LogLevel.h"

int main() {
    Logger logger("example.log", LogLevel::DEBUG);
    logger.log(LogLevel::DEBUG, "Debug message");
    logger.log(LogLevel::INFO, "Info message");
    logger.log(LogLevel::WARNING, "Warning message");
    logger.log(LogLevel::ERROR, "Error message");

    return 0;
}

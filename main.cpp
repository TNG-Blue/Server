#include <iostream>
#include <map>
#include <vector>
#include <fstream>
#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <sqlite3.h>
#include <cmath>

using namespace boost::asio;
using ip::tcp;

class SensorData {
public:
    double light_intensity{};
    double temperature{};
    double air_humidity{};
    double soil_humidity{};
    std::string timestamp;
};

class DeviceData {
public:
    std::string device_id;
    std::vector<SensorData> sensor_data_history;
};

class LoRaServer {
public:
    LoRaServer(const std::string& server_ip, unsigned short server_port)
            : acceptor_(io_service_, tcp::endpoint(ip::address::from_string(server_ip), server_port)) {
        std::cout << "Server IP address: " << server_ip << ", Port: " << server_port << std::endl;
    }

    void start() {
        create_sensor_data_table();

        while (true) {
            tcp::socket socket(io_service_);
            acceptor_.accept(socket);

            std::thread(&LoRaServer::handle_request, this, std::move(socket)).detach();
        }
    }

private:
    io_service io_service_;
    tcp::acceptor acceptor_;
    std::map<std::string, DeviceData> lora_devices;
    std::mutex devices_mutex;

    static void create_sensor_data_table() {
        sqlite3 *db;
        int rc = sqlite3_open("lora.db", &db);

        if (rc) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return;
        }

        std::string create_table_query = "CREATE TABLE IF NOT EXISTS sensor_data ("
                                         "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                         "device_id TEXT, "
                                         "light_intensity REAL, "
                                         "temperature REAL, "
                                         "air_humidity REAL, "
                                         "soil_humidity REAL, "
                                         "prediction TEXT, "
                                         "timestamp TEXT, "
                                         "note TEXT"
                                         ");";

        char *errmsg;
        rc = sqlite3_exec(db, create_table_query.c_str(), nullptr, nullptr, &errmsg);

        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << errmsg << std::endl;
            sqlite3_free(errmsg);
        }

        sqlite3_close(db);
    }

    std::vector<SensorData> get_training_data() {
        std::vector<SensorData> training_data;

        sqlite3 *db;
        int rc = sqlite3_open("lora.db", &db);
        if (rc) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return training_data;
        }

        std::string select_query = "SELECT temperature, prediction FROM sensor_data WHERE prediction IS NOT NULL;";

        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(db, select_query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return training_data;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SensorData data;
            data.temperature = sqlite3_column_double(stmt, 0);
            data.timestamp = "";
            data.light_intensity = 0.0;
            data.air_humidity = 0.0;
            data.soil_humidity = 0.0;
            const char* prediction = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

            training_data.push_back(data);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return training_data;
    }

    static std::string predict_environment(const SensorData& sensor_data, const std::vector<SensorData>& training_data, int k) {
        if (training_data.empty()) {
            return "unknown";
        }

        double min_distance = std::numeric_limits<double>::max();
        std::string prediction = "unknown";

        for (const SensorData& training_sample : training_data) {
            double distance = calculateEuclideanDistance(sensor_data, training_sample);

            if (distance < min_distance) {
                min_distance = distance;
                prediction = "good";
            }
        }

        return prediction;
    }

    static double calculateEuclideanDistance(const SensorData& data1, const SensorData& data2) {
        double distance = 0.0;

        distance += std::pow(data1.temperature - data2.temperature, 2);

        return std::sqrt(distance);
    }

    void update_sensor_data_with_prediction(const std::string& device_id, const SensorData& sensor_data) {
        std::vector<SensorData> training_data = get_training_data();

        std::string prediction = predict_environment(sensor_data, training_data, 3);

        std::string note = "";
        if (sensor_data.temperature > 30.0) {
            note += "High temperature; ";
        } else if (sensor_data.temperature < 10.0) {
            note += "Low temperature; ";
        }

        sqlite3 *db;
        int rc = sqlite3_open("lora.db", &db);
        if (rc) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return;
        }

        std::string insert_query = "INSERT INTO sensor_data (device_id, light_intensity, temperature, air_humidity, soil_humidity, timestamp, prediction, note) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return;
        }

        sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, sensor_data.light_intensity);
        sqlite3_bind_double(stmt, 3, sensor_data.temperature);
        sqlite3_bind_double(stmt, 4, sensor_data.air_humidity);
        sqlite3_bind_double(stmt, 5, sensor_data.soil_humidity);
        sqlite3_bind_text(stmt, 6, sensor_data.timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, prediction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, note.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL execution error: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void store_historical_data(const std::string& device_id, const SensorData& sensor_data) {
        std::lock_guard<std::mutex> lock(devices_mutex);
        if (lora_devices.find(device_id) != lora_devices.end()) {
            lora_devices[device_id].sensor_data_history.push_back(sensor_data);
        } else {
            DeviceData new_device_data;
            new_device_data.device_id = device_id;
            new_device_data.sensor_data_history = {sensor_data};
            lora_devices[device_id] = new_device_data;
        }

        std::ofstream logfile("log.txt", std::ios_base::app);
        if (logfile.is_open()) {
            logfile << "Received data from device " << device_id << ": "
                    << "Light Intensity: " << sensor_data.light_intensity << ", "
                    << "Temperature: " << sensor_data.temperature << ", "
                    << "Air Humidity: " << sensor_data.air_humidity << ", "
                    << "Soil Humidity: " << sensor_data.soil_humidity << " at timestamp "
                    << sensor_data.timestamp << std::endl;
            logfile.close();
        }
    }

    static void send_acknowledgment(tcp::socket& socket, const std::string& response) {
        boost::system::error_code error;
        socket.write_some(boost::asio::buffer(response), error);
    }

    void handle_request(tcp::socket socket) {
        boost::system::error_code error;

        tcp::endpoint remote_endpoint = socket.remote_endpoint();
        std::string client_ip = remote_endpoint.address().to_string();

        char buffer[1024];
        size_t len = socket.read_some(boost::asio::buffer(buffer), error);

        if (!error) {
            std::string data(buffer, len);

            size_t pos = data.find(':');
            if (pos != std::string::npos) {
                std::string device_id = data.substr(0, pos);
                std::string sensor_data_str = data.substr(pos + 1);

                SensorData sensor_data;
                if (sscanf(sensor_data_str.c_str(), "%lf %lf %lf %lf",
                           &sensor_data.light_intensity, &sensor_data.temperature,
                           &sensor_data.air_humidity, &sensor_data.soil_humidity) == 4) {
                    if (!device_id.empty()) {
                        sensor_data.timestamp = get_current_timestamp();
                        store_historical_data(device_id, sensor_data);
                        update_sensor_data_with_prediction(device_id, sensor_data);

                        std::cout << "Received data from device " << device_id << " at IP " << client_ip << ": " << std::endl;
                        std::cout << "  Light Intensity: " << sensor_data.light_intensity << std::endl;
                        std::cout << "  Temperature: " << sensor_data.temperature << std::endl;
                        std::cout << "  Air Humidity: " << sensor_data.air_humidity << std::endl;
                        std::cout << "  Soil Humidity: " << sensor_data.soil_humidity << std::endl;
                    }
                } else {
                    std::cerr << "Error parsing sensor data." << std::endl;
                }
            }
        }

        socket.close();
    }

    static std::string get_current_timestamp() {
        time_t now = time(0);
        tm* timestamp = localtime(&now);
        char timestamp_str[20];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", timestamp);
        return timestamp_str;
    }
};

int main() {
    std::string server_ip = "192.168.38.240";
    unsigned short server_port = 12345;

    LoRaServer server(server_ip, server_port);
    server.start();

    return 0;
}

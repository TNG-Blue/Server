#include <iostream>
#include <map> // Thư viện cho các loại ánh xạ (mapping).
#include <vector> // Thư viện cho các loại vector.
#include <fstream> // Thư viện cho việc nhập xuất dữ liệu từ và đến tệp tin.
#include <boost/asio.hpp> // Thư viện Boost.ASIO cho lập trình mạng và bất đồng bộ.
#include <thread> // Thư viện cho quản lý luồng (threads).
#include <mutex> // Thư viện cho quản lý mutex (đồng bộ hóa).
#include <sqlite3.h> // Thư viện cho quản lý cơ sở dữ liệu SQLite.
#include <cmath> // Thư viện cho các phép toán toán học.
#include <algorithm> // Thư viện cho các phép toán trên dãy số hoặc dãy phần tử.
#include <cstdlib> // Thư viện cho các chức năng hệ thống và chuỗi ngẫu nhiên.
#include <iomanip> // Thư viện cho định dạng và đầu ra đẹp hơn.

using namespace boost::asio; // Sử dụng không gian tên boost::asio cho đồng bộ hóa mạng.
using ip::tcp; // Sử dụng giao thức TCP/IP.

class SensorData { // Định nghĩa lớp SensorData cho dữ liệu cảm biến.
public:
    double light_intensity{}; // Độ sáng ánh sáng.
    double temperature{}; // Nhiệt độ.
    double air_humidity{}; // Độ ẩm không khí.
    double soil_humidity{}; // Độ ẩm đất.
    std::string timestamp; // Dấu thời gian.
    std::string prediction; // Dự đoán.
    std::string note; // Ghi chú.
};

class UserControlData { // Định nghĩa lớp UserControlData cho dữ liệu điều khiển người dùng.
public:
    int id; // ID.
    std::string device_id; // ID thiết bị.
    std::string command; // Lệnh điều khiển.
    std::string timestamp; // Dấu thời gian.

    UserControlData() : id(0) {} // Hàm tạo mặc định.
};

class DeviceData { // Định nghĩa lớp DeviceData cho dữ liệu thiết bị.
public:
    std::string device_id; // ID thiết bị.
    std::vector<SensorData> sensor_data_history; // Lịch sử dữ liệu cảm biến.
};

class LoRaServer { // Định nghĩa lớp LoRaServer cho máy chủ LoRa.
public:
    LoRaServer(const std::string& server_ip, unsigned short server_port)
            : acceptor_(io_service_, tcp::endpoint(ip::address::from_string(server_ip), server_port)) {
        std::cout << "Server IP address: " << server_ip << ", Port: " << server_port << std::endl; // In địa chỉ IP và cổng máy chủ.
    }

    void start() { // Bắt đầu máy chủ.
        create_sensor_data_table(); // Tạo bảng dữ liệu cảm biến.

        while (true) { // Vòng lặp vô hạn.
            tcp::socket socket(io_service_); // Tạo socket TCP.
            acceptor_.accept(socket); // Chấp nhận kết nối từ client.

            std::thread(&LoRaServer::handle_request, this, std::move(socket)).detach(); // Bắt đầu một luồng mới để xử lý yêu cầu từ client.
        }
    }

private:
    io_service io_service_; // Đối tượng io_service cho việc quản lý I/O bất đồng bộ.
    tcp::acceptor acceptor_; // Đối tượng acceptor cho việc chấp nhận kết nối từ client.
    std::map<std::string, DeviceData> lora_devices; // Map lưu trữ thông tin thiết bị LoRa.
    std::mutex devices_mutex; // Mutex để đồng bộ hóa truy cập đối tượng thiết bị.

    static void create_sensor_data_table() {
        sqlite3 *db; // Con trỏ đối tượng cơ sở dữ liệu SQLite.
        int rc = sqlite3_open("lora.db", &db); // Mở hoặc tạo cơ sở dữ liệu "lora.db".

        if (rc) {
            std::cerr << "Không thể mở cơ sở dữ liệu: " << sqlite3_errmsg(db) << std::endl; // In lỗi nếu không thể mở cơ sở dữ liệu.
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

        // Thêm tạo bảng user_control
        create_table_query += "CREATE TABLE IF NOT EXISTS user_control ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                              "device_id TEXT, "
                              "command TEXT, "
                              "timestamp TEXT"
                              ");";

        char *errmsg; // Con trỏ chuỗi cho thông báo lỗi từ SQLite.
        rc = sqlite3_exec(db, create_table_query.c_str(), nullptr, nullptr, &errmsg); // Thực hiện câu lệnh tạo bảng.

        if (rc != SQLITE_OK) {
            std::cerr << "Lỗi SQL: " << errmsg << std::endl; // In thông báo lỗi nếu có lỗi SQL.
            sqlite3_free(errmsg);
        }

        sqlite3_close(db); // Đóng cơ sở dữ liệu sau khi hoàn thành công việc.
    }


    static std::vector<SensorData> get_training_data() {
        std::vector<SensorData> training_data; // Vector lưu trữ dữ liệu huấn luyện.

        sqlite3 *db; // Con trỏ đối tượng cơ sở dữ liệu SQLite.
        int rc = sqlite3_open("lora.db", &db); // Mở cơ sở dữ liệu "lora.db".

        if (rc) {
            std::cerr << "Không thể mở cơ sở dữ liệu: " << sqlite3_errmsg(db) << std::endl; // In lỗi nếu không thể mở cơ sở dữ liệu.
            sqlite3_close(db);
            return training_data;
        }

        std::string select_query = "SELECT temperature FROM sensor_data WHERE prediction IS NOT NULL;"; // Câu lệnh truy vấn dữ liệu.

        sqlite3_stmt *stmt; // Đối tượng lưu trữ câu lệnh SQL đã được biên dịch.
        rc = sqlite3_prepare_v2(db, select_query.c_str(), -1, &stmt, nullptr); // Chuẩn bị câu lệnh truy vấn.

        if (rc != SQLITE_OK) {
            std::cerr << "Lỗi chuẩn bị SQL: " << sqlite3_errmsg(db) << std::endl; // In lỗi nếu có lỗi chuẩn bị SQL.
            sqlite3_close(db);
            return training_data;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SensorData data; // Đối tượng lưu trữ dữ liệu cảm biến.
            data.temperature = sqlite3_column_double(stmt, 0); // Lấy giá trị nhiệt độ từ cột 0.

            // Thiết lập các giá trị còn lại của SensorData mặc định là 0 hoặc chuỗi rỗng.
            data.timestamp = "";
            data.light_intensity = 0.0;
            data.air_humidity = 0.0;
            data.soil_humidity = 0.0;

            training_data.push_back(data); // Thêm dữ liệu vào vector huấn luyện.
        }

        sqlite3_finalize(stmt); // Giải phóng bộ nhớ đã được cấp phát cho câu lệnh SQL.
        sqlite3_close(db); // Đóng cơ sở dữ liệu sau khi hoàn thành công việc.

        return training_data; // Trả về vector chứa dữ liệu huấn luyện.
    }

    static std::string predict_environment(const SensorData& sensor_data, const std::vector<SensorData>& training_data, int k) {
        // Nếu không có dữ liệu huấn luyện, trả về "unknown".
//    if (training_data.empty()) {
//        return "unknown";
//    }

        bool all_conditions_met = true; // Biến kiểm tra tất cả điều kiện đã được thỏa mãn.
        bool is_temperature_valid = false; // Biến kiểm tra nhiệt độ có hợp lệ không.
        bool is_soil_humid = false; // Biến kiểm tra độ ẩm đất có hợp lệ không.
        std::string prediction = "good"; // Dự đoán ban đầu là "good" (tốt).

        // Thiết lập ngưỡng nhiệt độ và độ ẩm cho ban ngày và ban đêm.
        const double min_temperature_day = 24.0;
        const double max_temperature_day = 29.0;
        const double min_temperature_night = 16.0;
        const double max_temperature_night = 24.0;
        const double min_air_humidity = 60.0;
        const double max_air_humidity = 80.0;
        const double min_soil_humidity = 60.0;
        const double max_soil_humidity = 70.0;
        const double min_light_intensity = 1500.0;
        const double max_light_intensity = 2000.0;

        int current_hour = getHourFromTimestamp(sensor_data.timestamp); // Lấy giờ hiện tại từ dấu thời gian.

        bool is_daytime = (current_hour >= 6 && current_hour < 18); // Kiểm tra xem có phải ban ngày không.

        double min_distance = std::numeric_limits<double>::max(); // Khởi tạo giá trị khoảng cách nhỏ nhất ban đầu.

        // Duyệt qua từng mẫu dữ liệu trong dữ liệu huấn luyện.
        for (const SensorData& training_sample : training_data) {

            int training_hour = getHourFromTimestamp(training_sample.timestamp); // Lấy giờ từ dấu thời gian của mẫu huấn luyện.

            bool is_air_humid = (sensor_data.air_humidity >= min_air_humidity && sensor_data.air_humidity <= max_air_humidity); // Kiểm tra độ ẩm không khí có nằm trong ngưỡng không.
            bool is_light_sufficient = (sensor_data.light_intensity >= min_light_intensity && sensor_data.light_intensity <= max_light_intensity); // Kiểm tra độ sáng ánh sáng có đủ.

            if (is_daytime) {
                is_temperature_valid = (sensor_data.temperature >= min_temperature_day && sensor_data.temperature <= max_temperature_day); // Kiểm tra nhiệt độ ban ngày.
            } else {
                is_temperature_valid = (sensor_data.temperature >= min_temperature_night && sensor_data.temperature <= max_temperature_night); // Kiểm tra nhiệt độ ban đêm.
            }

            is_soil_humid = (sensor_data.soil_humidity >= min_soil_humidity && sensor_data.soil_humidity <= max_soil_humidity); // Kiểm tra độ ẩm đất có nằm trong ngưỡng không.

            bool conditions_met = is_temperature_valid && is_air_humid && is_soil_humid && is_light_sufficient; // Kiểm tra xem tất cả điều kiện đã được thỏa mãn.

            if (conditions_met) {
                double distance = calculateEuclideanDistance(sensor_data, training_sample); // Tính khoảng cách Euclidean giữa dữ liệu cảm biến và mẫu huấn luyện.

                if (distance < min_distance) {
                    min_distance = distance;
                    prediction = "good"; // Cập nhật dự đoán nếu tìm thấy khoảng cách nhỏ hơn.
                }
            } else {
                all_conditions_met = false; // Nếu không thỏa mãn một số điều kiện, đặt biến này thành "false".
            }
        }

        // Nếu dự đoán vẫn là "good" và không thỏa mãn tất cả điều kiện, cập nhật dự đoán thành "bad".
        if (prediction == "good" && !all_conditions_met) {
            prediction = "bad";
        }

        return prediction; // Trả về dự đoán cuối cùng.
    }

    static bool is_daytime_training(int training_hour) {
        // Xác định giờ nào được coi là buổi sáng trong dữ liệu huấn luyện

        return (training_hour >= 6 && training_hour < 18);
    }

    static bool is_nighttime_training(int training_hour) {
        // Xác định giờ nào được coi là buổi tối trong dữ liệu huấn luyện,

        return (training_hour >= 18 || training_hour < 6);
    }

    static int getHourFromTimestamp(const std::string& timestamp) {
        // Phân tích thời gian từ timestamp và lấy giờ
        std::tm tm_time = {};
        std::istringstream ss(timestamp);
        ss >> std::get_time(&tm_time, "%Y-%m-%d %H:%M:%S");
        return tm_time.tm_hour;
    }

    static double calculateEuclideanDistance(const SensorData& data1, const SensorData& data2) {
        double distance = 0.0;

        distance += std::pow(data1.temperature - data2.temperature, 2);

        return std::sqrt(distance);
    }

    static void update_sensor_data_with_prediction(const std::string& device_id, SensorData& sensor_data) {
        std::vector<SensorData> training_data = get_training_data();

        std::string prediction = predict_environment(sensor_data, training_data, 3);

        // Kiểm tra nhiệt độ
        if (sensor_data.temperature > 30.0) {
            sensor_data.note += "High temperature; ";
        } else if (sensor_data.temperature < 10.0) {
            sensor_data.note += "Low temperature; ";
        }

        // Kiểm tra ánh sáng
        if (sensor_data.light_intensity < 1500.0 || sensor_data.light_intensity > 2000.0) {
            sensor_data.note += "Unusual light intensity; ";
        }

        // Kiểm tra độ ẩm không khí
        if (sensor_data.air_humidity < 60.0 || sensor_data.air_humidity > 80.0) {
            sensor_data.note += "Air humidity out of range; ";
        }

        // Kiểm tra độ ẩm đất
        if (sensor_data.soil_humidity < 60.0 || sensor_data.soil_humidity > 70.0) {
            sensor_data.note += "Soil humidity out of range; ";
        }

        if (prediction == "good") {
            sensor_data.note = "";
        }

        // Cập nhật cơ sở dữ liệu
        sqlite3 *db;
        int rc = sqlite3_open("lora.db", &db); // Mở hoặc tạo cơ sở dữ liệu "lora.db".
        if (rc) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl; // In lỗi nếu không thể mở cơ sở dữ liệu.
            sqlite3_close(db);
            return;
        }

        std::string insert_query = "INSERT INTO sensor_data (device_id, light_intensity, temperature, air_humidity, soil_humidity, timestamp, prediction, note) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr); // Chuẩn bị câu lệnh SQL.

        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return;
        }
        // Gắn giá trị vào câu lệnh SQL.
        sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, sensor_data.light_intensity);
        sqlite3_bind_double(stmt, 3, sensor_data.temperature);
        sqlite3_bind_double(stmt, 4, sensor_data.air_humidity);
        sqlite3_bind_double(stmt, 5, sensor_data.soil_humidity);
        sqlite3_bind_text(stmt, 6, sensor_data.timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, prediction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, sensor_data.note.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL execution error: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt); // Giải phóng bộ nhớ đã được cấp phát cho câu lệnh SQL.
        sqlite3_close(db); // Đóng cơ sở dữ liệu sau khi hoàn thành công việc.
    }

    void store_historical_data(const std::string& device_id, const SensorData& sensor_data) {
        // Khóa mutex để tránh xung đột dữ liệu giữa các luồng
        std::lock_guard<std::mutex> lock(devices_mutex);
        // Nếu thiết bị đã tồn tại trong danh sách, thêm dữ liệu cảm biến vào lịch sử của nó
        if (lora_devices.find(device_id) != lora_devices.end()) {
            lora_devices[device_id].sensor_data_history.push_back(sensor_data);
        } else {
            // Nếu không, tạo một thiết bị mới và thêm vào danh sách
            DeviceData new_device_data;
            new_device_data.device_id = device_id;
            new_device_data.sensor_data_history = {sensor_data};
            lora_devices[device_id] = new_device_data;
        }

        // Mở tệp log.txt và ghi dữ liệu cảm biến nhận được vào tệp
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

    // Hàm gửi phản hồi đến thiết bị gửi dữ liệu
    static void send_acknowledgment(tcp::socket& socket, const std::string& response) {
        boost::system::error_code error;
        socket.write_some(boost::asio::buffer(response), error);
    }

    void handle_request(tcp::socket socket) {
        boost::system::error_code error;

        // Xác định địa chỉ IP của thiết bị gửi dữ liệu
        tcp::endpoint remote_endpoint = socket.remote_endpoint();
        std::string client_ip = remote_endpoint.address().to_string();

        char buffer[1024];
        size_t len = socket.read_some(boost::asio::buffer(buffer), error);

        if (!error) {
            std::string data(buffer, len);

            size_t pos = data.find(':');
            if (pos != std::string::npos) {
                // Tách chuỗi dữ liệu thành ID thiết bị và dữ liệu cảm biến

                std::string device_id = data.substr(0, pos);
                std::string sensor_data_str = data.substr(pos + 1);

                SensorData sensor_data;
                // Phân tích dữ liệu cảm biến từ chuỗi và lưu vào biến sensor_data
                if (sscanf(sensor_data_str.c_str(), "%lf %lf %lf %lf",
                           &sensor_data.light_intensity, &sensor_data.temperature,
                           &sensor_data.air_humidity, &sensor_data.soil_humidity) == 4) {
                    if (!device_id.empty()) {
                        // Lấy thời điểm hiện tại và lưu dữ liệu vào lịch sử và dự đoán cảm biến
                        sensor_data.timestamp = get_current_timestamp();
                        store_historical_data(device_id, sensor_data);
                        update_sensor_data_with_prediction(device_id, sensor_data);


                        // In thông tin dữ liệu cảm biến nhận được ra màn hình
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

    // Lấy thời điểm hiện tại dưới dạng chuỗi
    static std::string get_current_timestamp() {
        time_t now = time(0);
        tm* timestamp = localtime(&now);
        char timestamp_str[20];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", timestamp);
        return timestamp_str;
    }
};

void runPythonScript(const char* scriptPath) {
    // Xây dựng lệnh để chạy tệp script Python bằng lệnh "python"
    std::string command = "python " + std::string(scriptPath);

    // Sử dụng hàm std::system để thực thi lệnh Python
    int result = std::system(command.c_str());

    // Kiểm tra kết quả của việc thực thi lệnh
    if (result == 0) {
        // Nếu kết quả là 0, tức là lệnh đã chạy thành công
        std::cout << "Chạy thành công: " << scriptPath << std::endl;
    } else {
        // Nếu kết quả khác 0, có thể có lỗi xảy ra trong quá trình chạy lệnh
        std::cerr << "Lỗi khi chạy: " << scriptPath << std::endl;
    }
}

int main() {
    const char* testScriptPath = "F:/Source/C++/Database_Server/test.py";
    const char* mainScriptPath = "F:/Source/C++/Database_Server/api.py";

    std::string server_ip = "192.168.172.152";
    unsigned short server_port = 12345;

    LoRaServer server(server_ip, server_port);
    std::thread serverThread([&server]() { server.start(); });

    std::thread testThread(runPythonScript, testScriptPath);
    std::thread mainThread(runPythonScript, mainScriptPath);

    serverThread.join();
    testThread.join();
    mainThread.join();

    return 0;
}
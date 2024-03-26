#include "upload_module.h"
#include <regex>
#include <string>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#define MAX_BUFFER_SIZE 1024
#include <fstream>
#include <sstream>
#include <vector>
#include "config.h"

void parseFileName(const string& filename, string& measurement, string& imageNum, string& timestamp) {
    size_t first = filename.find("_");
    size_t second = filename.find("_", first+1);
    size_t third = filename.find("_", second+1);
    measurement = filename.substr(0,2);
    imageNum = filename.substr(second-1,1);
    timestamp = filename.substr(third+1,13);
}

int countUnderscores(const std::string& str) {
    int count = 0;
    for (char c : str) {
        if (c == '_') {
            count++;
        }
    }
    return count;
}

void process_csv_file(std::string path) {
  // csvファイルを開く
  std::ifstream csv_file(path);

  if (csv_file.is_open()) {
    // csvファイルを読み込む
    std::vector<std::vector<double>> data; // 二次元配列

    std::string line;
    while (std::getline(csv_file, line)) {
      std::vector<double> row;
      std::stringstream ss(line);
      std::string cell;

      while (std::getline(ss, cell, ',')) {
      row.push_back(std::stod(cell)); // 文字列をdoubleに変換
    }
    data.push_back(row);
  }

    csv_file.close();

    // すべての行にアクセスしてある動作を行う
    for (auto& row : data) {
        if(row[5] > 10000){
        int measurement = static_cast<int>(row[0]);
        double throughput = row[6];
        int timestamp = static_cast<int>(row[3]) + 9 * 3600;
        ostringstream cmdss;
        cmdss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '0" << measurement << " throughput=" << throughput << " " << timestamp << "'";
        system(cmdss.str().c_str());
        ostringstream sqlss;
        sqlss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s 'all_throughput throughput=" << throughput << " " << timestamp << "'";
        system(sqlss.str().c_str());
        }
    }

    // ファイルの中身を空にする
    std::ofstream ofs(path, std::ios::trunc); // ファイルを再オープンし、ファイルポインタを先頭に移動する
    ofs.close();
  } else {
    std::cout << "Failed to open file: " << path << std::endl;
  }
}


UploadModule::UploadModule(void)
{

}


UploadModule::~UploadModule(void)
{

}

time_t convert_to_unix(string time_str) {
    struct tm tm = {};
    if (strptime(time_str.c_str(), "%Y%m%d-%H%M", &tm) == nullptr) {
        return -1;
    }
    tm.tm_isdst = -1; // サマータイムを自動調整
    tm.tm_gmtoff = 9 * 3600; // 日本時間 (UTC+9)
    time_t timestamp = mktime(&tm);
    if (timestamp == -1) {
        return -1;
    }
    return timestamp;
}

//正常に実行できたらtrue
bool check_exist(std::string cli,std::string measurement,std::string timestamp,std::string filename){
    // influxDB2にデータをアップロードするコマンドを作成
    char command[MAX_BUFFER_SIZE];
    snprintf(command, MAX_BUFFER_SIZE, "%s",cli.c_str());

    // コマンドを実行し、標準出力をパイプとして取得する
    FILE* pipe = popen(command, "r");
    /*if (!pipe) {
        std::cout << "Failed to open pipe." << std::endl;
        return false;
    }*/

    // 外部コマンドの出力を1行ずつ読み込む
    char buffer[MAX_BUFFER_SIZE];
    std::string output = "";
    while (fgets(buffer, MAX_BUFFER_SIZE, pipe) != NULL) {
        output += buffer;
    }

    std::cout << output ;

    // 外部コマンドの出力にエラーメッセージが含まれるかどうかを判定する
    if (!output.empty()) {
        std::cout << "Error: Failed to upload data to influxDB2." << std::endl;
        fclose(pipe);
        ostringstream cmd_file;
        ostringstream cmd_error;
        cmd_file << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " error_filename=\"" << filename << "\" " << timestamp << "'";
        cmd_error << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " error_message=\"" << output << "\" " << timestamp << "'";
        system(cmd_file.str().c_str());
        system(cmd_error.str().c_str());
        return false;
    }

    // コマンドの戻り値を取得する
    int result = pclose(pipe);
    if (result == -1) {
        std::cout << "Error: Failed to get command return value." << std::endl;
        return false;
    }

    // コマンドの戻り値が0であれば成功と判定する
    if (WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        std::cout << "Upload data to influxDB2 succeeded." << std::endl;
        return true;
    }
    return false;
}

void UploadModule::uploadpsql(unsigned int devid,string filepath,FileInfo info)
{
    //file名形式(csv)
    //09_20230402-1000.csv
    //file名形式(jpg)
    //09_01_RGB_20230402-1000.jpg
    //path形式
    ///home/chikura/DaiwaComputer/20230331/09/image1
	ostringstream sqlss;
	ostringstream cmdss;
	time_t t = time(nullptr);
	const tm* lt = localtime(&t);
    
    //std::string measurement;
    std::regex re("^.*\\.csv$");
    int underNum = countUnderscores(info.filename);
    /*
    std::regex re1(".*image1/.*");
    std::regex re2(".*image2/.*");
	std::regex re3(".*image3/.*");
    std::regex re4(".*image4/.*");
    std::regex pattern("/(\\d+)/");
    //std::regex pattern("/(?:[^/]+/){4}([^/]+)/$");
    std::smatch match;
    std::regex_search(filepath, match, pattern);
    measurement = match[1].str();

    //file名から時間情報取得
    std::string filetime = info.filename.substr(0, 13);
    time_t u_time = convert_to_unix(filetime);
	u_time += 9*3600 -1;
	std::string timestamp = to_string(u_time);
    //ファイルの絶対パス取得
	char resolved_path[1000];
	char* result = realpath(filepath.c_str(), resolved_path);
	std::string abs_path = std::string(resolved_path);
    */
    //csvファイルの場合
	if(std::regex_match(info.filename, re)){
//influx write --org MinenoLaboratory --bucket HappyQuality --token EaEdvq6Lf0AudJn1K0uurpVQfeYGlNocKqJie7_eyWcq9dPcbfzaE6NwcDuLufNhdfi92Bg8nnDh9x6n7FcN1Q== --file ../data/01/sensor/20230222-1000.csv --format csv
	cmdss << "influx write --org " << ORGANIZATION << " --bucket " BUCKET  << " --file " << filepath << " --format=csv --token=" << TOKEN << " 2>&1";

    std::size_t pos = info.filename.find("_"); // "_"の位置を取得
    if (underNum == 1) { // "_"の数が1
        std::string measurement = info.filename.substr(0, 2); // "_"の前の2文字を抽出
        std::string filetime = info.filename.substr(pos + 1, 13); // "_"の後の13文字を抽出
        time_t u_time = convert_to_unix(filetime);
	    u_time += 9*3600 -1;
	    std::string timestamp = to_string(u_time);
        bool result = check_exist(cmdss.str().c_str(),measurement,timestamp,info.filename);
        if(result){
            remove(filepath.c_str());
            printf("remove csv file\n");
        } else {
            printf("dont remove csv file\n");
        }
    }
	} else {
        if(underNum == 3){
        //csv以外のファイル
        std::string measurement, imageNum, filetime;
        parseFileName(info.filename, measurement, imageNum, filetime);
        time_t u_time = convert_to_unix(filetime);
	    u_time += 9*3600 -1;
	    std::string timestamp = to_string(u_time);
        char resolved_path[1000];
	    char* result = realpath(filepath.c_str(), resolved_path);
	    std::string abs_path = std::string(resolved_path);
        cmdss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " image" << imageNum << "=\"" << abs_path << "\" " << timestamp << "'";
        printf(cmdss.str().c_str());
	    system(cmdss.str().c_str());
        if(MEASURE_THROUGHPUT){
            process_csv_file("./log/throughputlog.csv");
            }
        }
    }
    
    /*{
//influx write --org MinenoLaboratory --bucket HappyQuality --token EaEdvq6Lf0AudJn1K0uurpVQfeYGlNocKqJie7_eyWcq9dPcbfzaE6NwcDuLufNhdfi92Bg8nnDh9x6n7FcN1Q== 'ImageUploadTest3 back_image_path="test"'
        if(std::regex_match(filepath, re1)){
			cmdss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " image1=\"" << abs_path << "\" " << timestamp << "'";
	    	system(cmdss.str().c_str());
        } else if(std::regex_match(filepath, re2)){
			cmdss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " image2=\"" << abs_path << "\" " << timestamp << "'";
            system(cmdss.str().c_str());
        } else if(std::regex_match(filepath, re3)){
            cmdss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " image3=\"" << abs_path << "\" " << timestamp << "'";
            system(cmdss.str().c_str());
        } else if(std::regex_match(filepath, re4)){
            cmdss << "influx write --org " << ORGANIZATION << " --bucket " << BUCKET << " --token " << TOKEN << " -p s '" << measurement << " image4=\"" << abs_path << "\" " << timestamp << "'";
            system(cmdss.str().c_str());
        }
        if(MEASURE_THROUGHPUT){
            process_csv_file("./log/throughputlog.csv");
            }
	}*/
}

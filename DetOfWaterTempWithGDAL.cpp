#include <iostream>
#include "gdal_priv.h"
#include <fstream>
#include <string>
#include <random>
#include <numeric>
#include <vector>
#include <Windows.h>

class LandsatScanner {

#define SCALE 0
#define OFFSET 1
#define K1 2
#define K2 3

private:
    int height = 0;
    int width = 0;
    uint16_t* array_B4 = nullptr;
    uint16_t* array_B5 = nullptr;
    uint16_t* array_B10 = nullptr;
    double* NDVI = nullptr;
    std::vector<double> values = { 0,0,0,0 };
    std::vector<int> distance = { 28,27,25,25 };
    std::vector<std::string> parameters = {
        "RADIANCE_MULT_BAND_10",
        "RADIANCE_ADD_BAND_10",
        "K1_CONSTANT_BAND_10",
        "K2_CONSTANT_BAND_10",
    };
    void check(GDALMajorObject* obj, std::string errorMessage) {
        if (!obj) {
            std::cout << errorMessage << std::endl;
            GDALDestroyDriverManager();
            GDALClose(obj);
            exit(1);
        }
    }
    void check(int status, std::string errorMessage) {
        if (status) {
            std::cout << errorMessage << std::endl;
            GDALDestroyDriverManager();
            exit(1);
        }
    }
    uint16_t* loadImg(std::string band_path) {
        // Чтение датасета снимков
        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(band_path.c_str(), GA_ReadOnly));
        check(dataset, "Ошибка открытия снимка " + band_path);
        // Получение растров снимков
        GDALRasterBand* raster = dataset->GetRasterBand(1);
        // Получение размеров снимка
        width = raster->GetXSize();
        height = raster->GetYSize();
        uint16_t* array = new uint16_t[width * height];
        // Запись в массив значений яркости пикселов
        check(raster->RasterIO(GF_Read, 0, 0, width, height, array, width, height, GDT_Int16, 0, 0), "Ошибка при чтении данных со снимка " + band_path);
        GDALClose(dataset);
        return array;
    }
    void loadMetadata(std::string metadata_path) {
        std::ifstream metadata(metadata_path); 
        if (!metadata.is_open()) {
            std::cout << "Не удалось открыть файл метаданных." << std::endl;
            exit(1);
        }
        std::string buffer; int i = 0;
        while (getline(metadata, buffer) && i != 4) {
            size_t value = buffer.find(parameters[i]);
            if (value != std::string::npos) {
                values[i] = stod(buffer.substr(distance[i]));
                ++i;
            }
        }
    }
public:
    LandsatScanner(std::string metadata, std::string band4_path, std::string band5_path, std::string band10_path) {
        GDALAllRegister();
        array_B4 = loadImg(band4_path);
        array_B5 = loadImg(band5_path);
        array_B10 = loadImg(band10_path);
        loadMetadata(metadata);
    }
    double getK1() { return this->values[K1]; }
    double getK2() { return this->values[K2]; }
    double getScale() { return this->values[SCALE]; }
    double getOffset() { return this->values[OFFSET]; }
    void calculateNDVI() {
        // Находим значения NDVI 
        NDVI = new double[width * height];
        for (int i = 0; i < width * height; i++)
            NDVI[i] = (array_B4[i] + array_B5[i]) ? double(array_B5[i] - array_B4[i]) / (array_B5[i] + array_B4[i]) : 1;
    }
    double calculateTemperature(int x, int y) {
        if (!NDVI) calculateNDVI();
        int pixel = (y - 1) * width + x;
        if (NDVI[pixel] < 0)
            return values[K2] / log((values[K1] / (values[SCALE] * array_B10[pixel] + values[OFFSET])) + 1) - 273.15;
        else {
            std::cout << "Выбранный пиксель не принадлежит водоёму!" << std::endl;
            return values[K2] / log((values[K1] / (values[SCALE] * array_B10[pixel] + values[OFFSET])) + 1) - 273.15;
        }
    }
    double calculateAverageTemperature() {
        if (!NDVI) calculateNDVI();
        //Считаем температуру
        double result = 0; int counter = 0;
        for (int i = 0; i < width * height; i++) {
            if (NDVI[i] < 0) {
                result += values[K2] / log((values[K1] / (values[SCALE] * array_B10[i] + values[OFFSET])) + 1);
                ++counter;
            }
        }
        return double(result / counter - 273.15);
    }
};

int main() {

    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    std::string band4 = "SatelliteImg/LC09_L1TP_184017_20220828_20230331_02_T1_B4.tif";
    std::string band5 = "SatelliteImg/LC09_L1TP_184017_20220828_20230331_02_T1_B5.tif";
    std::string band10 = "SatelliteImg/LC09_L1TP_184017_20220828_20230331_02_T1_B10.tif";
    std::string metadata = "SatelliteImg/LC09_L1TP_184017_20220828_20230331_02_T1_MTL.txt";
    LandsatScanner scanner(metadata, band4, band5, band10);
    std::cout << "Значения метадаты:"
        << "\n\tk1 = " << scanner.getK1()
        << "\n\tk2 = " << scanner.getK2()
        << "\n\tscale = " << scanner.getScale()
        << "\n\toffset = " << scanner.getOffset() << std::endl;
        
    std::cout << "Средняя температура: " << scanner.calculateAverageTemperature() << std::endl;
    int x, y;
    std::cout << "Проверка вычислений. Введите значение по Х от 0 до 8251: "; std::cin >> x;
    std::cout << "Введите значение по Y от 0 до 8301: "; std::cin >> y;
    std::cout << "Температура пикселя " << x << ":" << y << " = " << scanner.calculateTemperature(x, y);
}
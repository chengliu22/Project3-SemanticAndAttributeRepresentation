﻿#include <Feature/ColorHistogram.hpp>
#include <Feature/HistogramOfOrientedGradients.hpp>
#include <BagOfFeatures/Codewords.hpp>
#include <Quantization/HardAssignment.hpp>
#include <Quantization/VocabularyTreeQuantization.hpp>
#include <Util/Clustering.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <map>
#include <time.h>
#include <opencv2/opencv.hpp>

using namespace ColorTextureShape;
using namespace LocalDescriptorAndBagOfFeature;

cv::Size regionSize(54, 54); // Default HoG is 3x3 cell blocks of 6x6 pixel cells, this gives 3x3 block regions

std::vector<cv::Mat> load_images(std::string imageFileList)
{
    std::ifstream iss(imageFileList);
    
    std::vector<cv::Mat> images;
    while(iss)
    {
        std::string imFile;
        std::getline(iss,imFile);
        
        cv::Mat img = cv::imread(imFile);
        if(img.data != NULL)
            images.push_back(img);
    }
    
    return images;
}

int main(int argc, char **argv)
{
    // Feature set
    std::vector<HistogramFeature *> features; 
    features.push_back(new HistogramOfOrientedGradients());
    features.push_back(new ColorHistogram());
    
    // Load input images
    std::vector<cv::Mat> posImages = load_images(argv[1]);  
    std::vector<cv::Mat> negImages = load_images(argv[2]);
    std::map<cv::Mat *, std::vector<std::vector<double>>> featuresForImages;
    
    // Extract HoG features and Color Histograms using early fusion
    for(cv::Mat &img : posImages)
    {
        std::vector<std::vector<double>> imgFeatures;
        for(int j = 0; j < img.rows - regionSize.height + 1; j+=regionSize.height)
        {
            for(int i = 0; i <  img.cols - regionSize.width + 1; i+=regionSize.width)
            {
                cv::Mat region = img(cv::Rect(i, j, regionSize.width, regionSize.height));
                
                std::vector<double> featureVector;                
                for(HistogramFeature *feat : features)
                {
                    std::vector<double> f = feat->Compute(region);
                    featureVector.insert(featureVector.end(), f.begin(), f.end());
                }
                
                imgFeatures.push_back(featureVector);             
            }
        }
        
        featuresForImages[&img] = imgFeatures;
    }
    
    for(cv::Mat &img : negImages)
    {
        std::vector<std::vector<double>> imgFeatures;
        for(int j = 0; j < img.rows - regionSize.height + 1; j+=regionSize.height)
        {
            for(int i = 0; i <  img.cols - regionSize.width + 1; i+=regionSize.width)
            {
                cv::Mat region = img(cv::Rect(i, j, regionSize.width, regionSize.height));
                
                std::vector<double> featureVector;                
                for(HistogramFeature *feat : features)
                {
                    std::vector<double> f = feat->Compute(region);
                    featureVector.insert(featureVector.end(), f.begin(), f.end());
                }
                
                imgFeatures.push_back(featureVector);             
            }
        }
        
        featuresForImages[&img] = imgFeatures;
    }
    
    for(HistogramFeature *feat : features)
    {
        delete feat;
    }
    
    // Create codebook
    std::vector<std::vector<double>> featureSet;
    for(auto &feat : featuresForImages)
    {
        for(auto fv : feat.second)
            featureSet.push_back(fv);
    }
    
/*
    std::vector<std::vector<double>> codebook;
    FindCodewords(featureSet, 5, codebook);
*/

    double start = clock();
    std::cout << "Build Vocabulary Tree" << std::endl;
    vocabulary_tree tree; //(4^4) = 256 words
    tree.K = 4; //branching factor
    tree.L = 4; //depth
    hierarchical_kmeans(featureSet, tree);
    std::cout << double( clock() - start ) / (double)CLOCKS_PER_SEC<< " seconds." << std::endl;

/*
    // Save codebook for later testing
    SaveCodebook("codebook", codebook);
*/
    SaveVocabularyTree("tree", tree);

    // Compute bag of features for each training image
/*
    HardAssignment quant(codebook);
*/
    VocabularyTreeQuantization quant(tree);
    std::map<cv::Mat *, std::vector<double>> trainingBoW;
    for(cv::Mat &img: posImages)
    {
        std::vector<double> bow;
        quant.quantize(featuresForImages[&img], bow);
        
        trainingBoW[&img] = bow;
    }
    
    for(cv::Mat &img: negImages)
    {
        std::vector<double> bow;
        quant.quantize(featuresForImages[&img], bow);
        
        trainingBoW[&img] = bow;
    }
    
    // Save the training file in LibSVM format
    std::ofstream trainingFile("train.out");
    for(cv::Mat &img : posImages)
    {
        trainingFile << "+1 ";
        
        for(int j = 0; j < trainingBoW[&img].size(); j++)
        {
            if(trainingBoW[&img][j] != 0)
            {
                trainingFile << j + 1 << ":" << trainingBoW[&img][j] << " ";
            }
        }
        
        trainingFile << std::endl;
    }
    
    for(cv::Mat &img : negImages)
    {
        trainingFile << "-1 ";
        
        for(int j = 0; j < trainingBoW[&img].size(); j++)
        {
            if(trainingBoW[&img][j] != 0)
            {
                trainingFile << j + 1 << ":" << trainingBoW[&img][j] << " ";
            }
        }
        
        trainingFile << std::endl;
    }
    trainingFile.close();
    return 0;
}

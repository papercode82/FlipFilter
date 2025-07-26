
#ifndef FREERS_H
#define FREERS_H

#include <cstdint>
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <vector>
#include <fstream>
#include <string>
#include "Sketch.h"


#define HASH_SEED_RS 51318471
using namespace std;

struct SSKeyNode;

struct SSValueNode {
    uint32_t counter;
    SSValueNode *next;
    SSValueNode *pre;
    SSKeyNode *firstKeyNode;
};

struct SSKeyNode {
    uint32_t key;
    uint32_t err;
    SSValueNode *parent;
    SSKeyNode *next;
};

class StreamSummary {
private:
    uint32_t size = 0;
    uint32_t SS_MAX_SIZE;
    SSValueNode *firstValueNode = nullptr;
    unordered_map<uint32_t, SSKeyNode *> hashTable;

public:
    StreamSummary(uint32_t memorySize) {
        SS_MAX_SIZE = memorySize * 1024 * 8 / 288;
    }

    ~StreamSummary(){
        SSValueNode *ssValueNode = firstValueNode;
        while (ssValueNode != nullptr) {
            SSKeyNode *ssKeyNode = ssValueNode->firstKeyNode;
            do {
                SSKeyNode *tempKeyNode = ssKeyNode;
                ssKeyNode = ssKeyNode->next;
                free(tempKeyNode);
            } while (ssKeyNode != ssValueNode->firstKeyNode);

            SSValueNode *tempValueNode = ssValueNode;
            ssValueNode = ssValueNode->next;
            free(tempValueNode);
        }
    }

    uint32_t getSize() const{
        return size;
    }
    uint32_t getMinVal() const{
        if (size != 0) {
            return firstValueNode->counter;
        } else {
            return -1;
        }
    }

    SSValueNode* getFirstValueNode() const{
        return firstValueNode;
    }

    SSKeyNode* getMinKeyNode() const{
        return firstValueNode->firstKeyNode;
    }
    SSKeyNode* findKey(uint32_t key) const{
        auto iter = hashTable.find(key);
        if (iter == hashTable.end()) {
            return nullptr;
        } else {
            return iter->second;
        }
    }

    void SSPush(uint32_t key, uint32_t value,uint32_t err) {
        SSKeyNode *newKeyNode;
        SSValueNode *newValueNode= nullptr;
        if(size>=SS_MAX_SIZE){//when stream summary is full, drop a key node of the first value node (the smallest value node)
            //delete the second key node (or the first key node when the first value node only has one key node)
            SSKeyNode *ssKeyNode = firstValueNode->firstKeyNode->next;
            auto iter = hashTable.find(ssKeyNode->key);
            hashTable.erase(iter);

            if(ssKeyNode->next==ssKeyNode){//the first value node only has one key node
                newValueNode=firstValueNode;
                firstValueNode= firstValueNode->next;//may be nullptr
                firstValueNode->pre= nullptr;
            }else{
                firstValueNode->firstKeyNode->next = ssKeyNode->next;
            }
            size--;
            newKeyNode=ssKeyNode;
            newKeyNode->err=err;
        }else{
            newKeyNode = (SSKeyNode *) calloc(1, sizeof(SSKeyNode));
            newKeyNode->err=err;
        }

        SSValueNode *lastValueNode= nullptr;
        SSValueNode *curValueNode=firstValueNode;
        while(curValueNode!= nullptr and curValueNode->counter<value){//find the first value node larger than value
            lastValueNode=curValueNode;
            curValueNode=curValueNode->next;
        }

        if(curValueNode== nullptr or curValueNode->counter>value)//need to create a new value Node between lastValueNode and curValueNode
        {
            if(newValueNode== nullptr){
                newValueNode = (SSValueNode *) calloc(1, sizeof(SSValueNode));
                newValueNode->firstKeyNode = newKeyNode;
            }

            newKeyNode->key=key;
            newKeyNode->parent = newValueNode;
            newKeyNode->next = newKeyNode;

            newValueNode->counter = value;
            newValueNode->pre = lastValueNode;
            newValueNode->next=curValueNode;

            if(curValueNode!= nullptr){
                curValueNode->pre=newValueNode;
            }

            if(lastValueNode== nullptr){//means newValueNode should be the firstValueNode
                firstValueNode = newValueNode;
            }else{
                lastValueNode->next=newValueNode;
            }

            hashTable[key] = newKeyNode;
            size++;
        }else{//value==curValueNode->counter
            //add into curValueNode
            //the key nodes form a circular list
            //compared to inserting the new key node as the first key node, inserting the new key node after the first key node can avoid check whether the ori first key node's next is itself
            newKeyNode->key=key;
            newKeyNode->parent = curValueNode;
            newKeyNode->next = curValueNode->firstKeyNode->next;
            curValueNode->firstKeyNode->next = newKeyNode;

            hashTable[key] = newKeyNode;
            size++;
        }
    }


    void SSUpdate(SSKeyNode *ssKeyNode, uint32_t newValue) {

        SSValueNode *newValueNode = nullptr;
        SSValueNode *lastValueNode = nullptr;
        SSValueNode *curValueNode = nullptr;
        if (ssKeyNode == nullptr || ssKeyNode->parent == nullptr) {
            std::cerr << "Error: Invalid SSKeyNode or SSKeyNode->parent" << std::endl;
            return;
        }

        if (ssKeyNode->next == ssKeyNode) {
            newValueNode = ssKeyNode->parent;
            lastValueNode = newValueNode->pre;
            curValueNode = newValueNode->next;

            if (lastValueNode != nullptr) {
                lastValueNode->next = curValueNode;
            }
            if (curValueNode != nullptr) {
                curValueNode->pre = lastValueNode;
            }
            if (firstValueNode == newValueNode) {
                firstValueNode = newValueNode->next;
                if (firstValueNode != nullptr) {
                    firstValueNode->pre = nullptr;
                }
            }
        }
        else {
            lastValueNode = ssKeyNode->parent;
            curValueNode = lastValueNode->next;

            SSKeyNode* nextKeyNode = ssKeyNode->next;
            uint32_t key = ssKeyNode->key;
            uint32_t err = ssKeyNode->err;

            ssKeyNode->key = nextKeyNode->key;
            nextKeyNode->key = key;
            ssKeyNode->err = nextKeyNode->err;
            nextKeyNode->err = err;

            hashTable[ssKeyNode->key] = ssKeyNode;
            hashTable[nextKeyNode->key] = nextKeyNode;

            ssKeyNode->next = nextKeyNode->next;
            if (nextKeyNode == lastValueNode->firstKeyNode) {
                lastValueNode->firstKeyNode = ssKeyNode;
            }

            ssKeyNode = nextKeyNode;
        }

        while (curValueNode != nullptr && curValueNode->counter < newValue) {
            lastValueNode = curValueNode;
            curValueNode = curValueNode->next;
        }

        if (curValueNode == nullptr || curValueNode->counter > newValue) {
            if (newValueNode == nullptr) {
                newValueNode = (SSValueNode *) calloc(1, sizeof(SSValueNode));
                if (newValueNode == nullptr) {
                    std::cerr << "Error: Memory allocation failed for newValueNode" << std::endl;
                    return;
                }
                newValueNode->firstKeyNode = ssKeyNode;
                ssKeyNode->parent = newValueNode;
                ssKeyNode->next = ssKeyNode;
            }

            newValueNode->counter = newValue;
            newValueNode->pre = lastValueNode;
            newValueNode->next = curValueNode;

            if (curValueNode != nullptr) {
                curValueNode->pre = newValueNode;
            }

            if (lastValueNode == nullptr) {
                firstValueNode = newValueNode;
            } else {
                lastValueNode->next = newValueNode;
            }
        } else {  // newValue == curValueNode->counter
            ssKeyNode->parent = curValueNode;
            ssKeyNode->next = curValueNode->firstKeyNode->next;
            curValueNode->firstKeyNode->next = ssKeyNode;

            if (newValueNode != nullptr) {
                free(newValueNode);
            }
        }
    }



    void getKeyVals(unordered_map<uint32_t,uint32_t>& keyVals) const{
        for(auto iter=hashTable.begin();iter!=hashTable.end();iter++){
            uint32_t key=iter->first;
            SSKeyNode* keyNode=iter->second;
            uint32_t value=keyNode->parent->counter-keyNode->err;
            keyVals[key]=value;
        }
    }
};


class FreeRS : public Sketch{
private:
    StreamSummary ss;
    uint8_t* regArray;
    uint32_t SS_MAX_SIZE;
    uint32_t REG_NUM;
    double updateProb = 1.0;

    inline uint32_t getRegValue(uint32_t regIdx){
        uint32_t regStartByteIdx=regIdx*5/8;
        uint32_t regStartBitIdx=regIdx*5%8;
        return ((*(uint16_t*)(regArray+regStartByteIdx))>>regStartBitIdx)&0x1F;
    }
    inline void setRegValue(uint32_t regIdx,uint32_t newValue){
        uint32_t regStartByteIdx=regIdx*5/8;
        uint32_t regStartBitIdx=regIdx*5%8;

        uint16_t* value_ptr=(uint16_t*)(regArray+regStartByteIdx);
        *value_ptr=(*value_ptr)&(~(((uint16_t)0x1F)<<regStartBitIdx));
        *value_ptr=(*value_ptr)|(newValue<<regStartBitIdx);
    }

public:
    FreeRS(uint32_t memorySize);
    FreeRS(const FreeRS& c);
    ~FreeRS();

    void insert(const std::pair<uint32_t, uint32_t> pkt);
    void update(const uint32_t key, const uint32_t element);
    uint32_t query(const uint32_t key);

    void getEstimatedFlowSpreads(std::unordered_map<uint32_t, uint32_t>& estimatedFlowSpreads) const;
    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);
    void SSDetection(
        const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi,
        const std::vector<uint32_t> thresholds
    );

    std::unordered_map<uint32_t, uint32_t> candidates();

};


#endif

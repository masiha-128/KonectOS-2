#pragma once
#include "../types.h"

struct Node{
        Node* next;   
        Node* last;   
        Node* parent;
        void* data;
        Node* lastNodeCreat; //only work for parent node
        Node* GetNode(uint64_t position);
        Node* Add(void* data);
        void ModifyData(void* data);
        void Delete();
        uint64_t GetSize();
        void FreeAllNode();
};

Node* CreatNode(void* data);
Node* GetNode(Node* MainNode, uint64_t position);
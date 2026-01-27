#include <string.h>
#include <stdlib.h>
#include "commons.h"

node *createNode(void *data, size_t allocSize)
{
    node *toInsert = (node *)malloc(sizeof(node));
    if(toInsert == NULL)
    {
    return NULL;
    }


    toInsert->data = malloc(allocSize);
    if(toInsert->data == NULL)
    {
    free(toInsert);
    return NULL;
    }

    memcpy(toInsert->data, data, allocSize);
    toInsert->next = NULL;

    return toInsert;
}

node** batchNodeCreate(void *data,size_t allocation_size,size_t size){
    node** toInsert = (node**)malloc(sizeof(node*)*size);

    if(toInsert == NULL)
    {
        return NULL;
    }
    
    for(size_t i=0;i<size;++i){
        toInsert[i] = (node*)malloc(sizeof(node));
        toInsert[i]->data = malloc(allocation_size);
        if(toInsert[i]->data == NULL)
        {
            free(toInsert[i]);
            return NULL;
        }

        memcpy(toInsert[i]->data, data+i*allocation_size, allocation_size);

        toInsert[i]->next = NULL;
        if (i!=0)
            toInsert[i-1]->next = toInsert[i];
    }
    return toInsert;
}

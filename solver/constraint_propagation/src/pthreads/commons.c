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


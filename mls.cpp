/* MLS - Mod Loader`s Settings */
/* A functionality to store game/mods settings in a better way */
/* (for those who doesn`t like 1000 config files) */

#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/listitem.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "mls.h"

extern bool g_bMLSOnlyManualSaves;

class MLSKeychain;

struct MLSStorage
{
    char szKey[MAX_KEY_LEN];
    char szValue[MAX_VAL_LEN];
};

MLSKeychain* listSets = NULL;
LIST_START(MLSKeychain)
    LIST_INITSTART(MLSKeychain)
        memset(storage.szKey, 0, sizeof(storage.szKey));
        memset(storage.szValue, 0, sizeof(storage.szValue));
    LIST_INITEND()

    MLSStorage storage;
LIST_END()

void MLS::LoadFile()
{
    char path[256];
    snprintf(path, sizeof(path), "%s/gamesave.mls", aml->GetAndroidDataPath());
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        logger->Error("An error (%d) opening MLS file!", errno, strerror(errno));
        return;
    }

    logger->Info("MLS file is opened, reading sets...");
    MLSKeychain* item;
    while(true)
    {
        item = new MLSKeychain;
        fread(&item->storage, sizeof(MLSStorage), 1, f);
        if(!feof(f))
        {
            item->Push(&listSets);
        }
        else
        {
            fclose(f);
            logger->Info("MLS file has been readed. Sets: %d", item->Count());
            return;
        }
    }
}
void MLS::SaveFile()
{
    char path[256];
    snprintf(path, sizeof(path), "%s/gamesave.mls", aml->GetAndroidDataPath());
    FILE* f = fopen(path, "wb");
    if(!f) return;

    LIST_FOR(listSets)
    {
        fwrite(&item->storage, sizeof(MLSStorage), 1, f);
    }
    fclose(f);
}
bool MLS::HasValue(const char* key)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key)) return true;
    }
    return false;
}
void MLS::DeleteValue(const char* key)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            item->Remove(&listSets);
            delete item;
            if(!g_bMLSOnlyManualSaves) SaveFile();
        }
    }
}

void MLS::SetInt(const char* key, int32_t val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            memset(item->storage.szValue, 0, sizeof(MLSStorage::szValue));
            snprintf(item->storage.szValue, sizeof(MLSStorage::szValue), "%d", val);
            if(!g_bMLSOnlyManualSaves) SaveFile();
            return;
        }
    }

    MLSKeychain* newitem = new MLSKeychain;
    snprintf(newitem->storage.szKey, sizeof(MLSStorage::szKey), "%s", key);
    snprintf(newitem->storage.szValue, sizeof(MLSStorage::szValue), "%d", val);
    newitem->Push(&listSets);

    if(!g_bMLSOnlyManualSaves) SaveFile();
}
void MLS::SetFloat(const char* key, float val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            memset(item->storage.szValue, 0, sizeof(MLSStorage::szValue));
            snprintf(item->storage.szValue, sizeof(MLSStorage::szValue), "%8.6f", val);
            if(!g_bMLSOnlyManualSaves) SaveFile();
            return;
        }
    }

    MLSKeychain* newitem = new MLSKeychain;
    snprintf(newitem->storage.szKey, sizeof(MLSStorage::szKey), "%s", key);
    snprintf(newitem->storage.szValue, sizeof(MLSStorage::szValue), "%8.6f", val);
    newitem->Push(&listSets);

    if(!g_bMLSOnlyManualSaves) SaveFile();
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat" // dumb ass clang cries about "ld needs to be lld" and "lld needs to be ld" at the SAME TIME
void MLS::SetInt64(const char* key, int64_t val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            memset(item->storage.szValue, 0, sizeof(MLSStorage::szValue));
            snprintf(item->storage.szValue, sizeof(MLSStorage::szValue), "%lld", val);
            if(!g_bMLSOnlyManualSaves) SaveFile();
            return;
        }
    }

    MLSKeychain* newitem = new MLSKeychain;
    snprintf(newitem->storage.szKey, sizeof(MLSStorage::szKey), "%s", key);
    snprintf(newitem->storage.szValue, sizeof(MLSStorage::szValue), "%lld", val);
    newitem->Push(&listSets);

    if(!g_bMLSOnlyManualSaves) SaveFile();
}
#pragma clang diagnostic pop
void MLS::SetStr(const char* key, const char *val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            memset(item->storage.szValue, 0, sizeof(MLSStorage::szValue));
            snprintf(item->storage.szValue, sizeof(MLSStorage::szValue), "%s", val);
            if(!g_bMLSOnlyManualSaves) SaveFile();
            return;
        }
    }

    MLSKeychain* newitem = new MLSKeychain;
    snprintf(newitem->storage.szKey, sizeof(MLSStorage::szKey), "%s", key);
    snprintf(newitem->storage.szValue, sizeof(MLSStorage::szValue), "%s", val);
    newitem->Push(&listSets);

    if(!g_bMLSOnlyManualSaves) SaveFile();
}

bool MLS::GetInt(const char* key, int32_t *val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            *val = atoi(item->storage.szValue);
            return true;
        }
    }
    return false;
}
bool MLS::GetFloat(const char* key, float *val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            *val = atof(item->storage.szValue);
            return true;
        }
    }
    return false;
}
bool MLS::GetInt64(const char* key, int64_t *val)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            *val = atoll(item->storage.szValue);
            return true;
        }
    }
    return false;
}
bool MLS::GetStr(const char* key, char *val, size_t len)
{
    LIST_FOR(listSets)
    {
        if(!strcmp(item->storage.szKey, key))
        {
            memset(val, 0, len);
            snprintf(val, len, "%s", item->storage.szValue);
            return true;
        }
    }
    return false;
}
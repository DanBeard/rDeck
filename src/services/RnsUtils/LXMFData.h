#pragma once
#include <string.h>
#include <sys/time.h>
#include <set>
#include "Reticulum.h"

// data classes and serialization/deserialization helper for LXMF data

// up this every time you change a schema. It means we wipe the data but avoid corruption
#define LXMF_SCHEMA_VERSION 1
// Reminder: Keep alignment in mind. This is not packed on purpose for code size and speed
#define RNS_HASH_SIZE_BYTES 16
#define RNS_APP_DATA_SIZE_BYTES  285
#define NUM_ANNOUNCES 100

using namespace std;

namespace Retcon::LXMF {
    class AnnounceData {
        public:
            AnnounceData(JsonArray &array);
            AnnounceData(const RNS::Bytes &dest, const RNS::Bytes &app_data ,const time_t last_heard);
            RNS::Bytes dest;
            RNS::Bytes app_data;
            time_t last_heard;

            void serialize(JsonArray &array);

            boolean operator<(const AnnounceData& other) const {
                return this->last_heard < other.last_heard;
            }
             boolean operator>(const AnnounceData& other) const {
                return this->last_heard > other.last_heard;
            }
             boolean operator==(const AnnounceData& other) const {
                return last_heard == other.last_heard && dest == other.dest && app_data == other.app_data;
            }


    };

    class Message {
        public:
            Message(RNS::Bytes msg);
            RNS::Bytes src;
            RNS::Bytes dest;
            RNS::Bytes signature;
            RNS::Bytes packed_payload;

            RNS::Bytes fullMsg() const;
            void serialize(JsonArray array);
    };

    class Conversation {
        protected:
            std::list<Message> msgs; // use the accessors please :)
        public:
            static const uint32_t max_messages = 200;
            RNS::Bytes their_hash;
            string their_name;
        
            void addMessage(const Message &msg);
            const std::list<Message>& getMessages() const;

            void clear();
            void serialize(JsonDocument &doc); 
            void deserialize(JsonDocument &doc);

    };

    set<AnnounceData>* getAnnounceData();
    void persistAnnounceData();

    // only one conversation at a time
    Conversation* loadAsCurrentConversation(const RNS::Bytes &src_hash);
    void persistCurrentConversation();
    void addMessageToConversation(const Message &msg);
}





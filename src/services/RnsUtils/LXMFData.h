#pragma once
#include <string.h>
#include <sys/time.h>
#include <set>
#include "Reticulum.h"

#ifdef RET_PLATFORM_EMU
#include <Arduino.h>
#endif

// data classes and serialization/deserialization helper for LXMF data

// up this every time you change a schema. It means we wipe the data but avoid corruption
#define LXMF_SCHEMA_VERSION 3
// Reminder: Keep alignment in mind. This is not packed on purpose for code size and speed
#define RNS_HASH_SIZE_BYTES 16
#define RNS_APP_DATA_SIZE_BYTES  285
#define NUM_ANNOUNCES 100

using namespace std;

namespace Retcon::LXMF {
    class AnnounceData {
        public:
            AnnounceData(JsonArray &array);
            AnnounceData(const RNS::Bytes &dest, const RNS::Bytes &app_data, const time_t last_heard);
            AnnounceData(const RNS::Bytes &dest, const RNS::Bytes &app_data, const RNS::Bytes &public_key, const time_t last_heard);
            RNS::Bytes dest;
            RNS::Bytes app_data;
            RNS::Bytes public_key;
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

            string displayName() const {
                if ( app_data.size() > 3 && (( app_data.data()[0] >= 0x90 && app_data.data()[0] <= 0x9f) || app_data.data()[0] == 0xdc)) {
                    JsonDocument doc;
                    deserializeMsgPack(doc, app_data.data(), app_data.size());
                    if(doc.is<JsonArray>()) {
                        if(doc[0].is<MsgPackBinary>()) {
                            MsgPackBinary nameBin = doc[0].as<MsgPackBinary>();
                            if(nameBin.size() > 0) {
                                // this should add the /0 ... right?
                            return string((const char*)nameBin.data(), nameBin.size());
                        } 
                        }
                       
                        
                        if(doc[0].is<string>()) {
                            string nameStr = doc[0].as<string>();
                            if(nameStr.size() > 0) {
                                return nameStr;
                            }
                        }
                        // welp, dunno so just fall down to the hex
                    }

                }
                // if we can't find a name in app_data then just the hex *shrug*
                return dest.toHex();

            }
    };

    class Message {
        public:
            // referenced from https://github.com/markqvist/LXMF/blob/master/LXMF/LXMessage.py#L84
            const static size_t max_lxmf_payload_size = 435;
            enum STATUS {
                UNSET = 0,
                QUEUEING,
                SENDING,
                RETRY, // retrying
                
                COMPLETE_START,
                SENT, // direct send, confirmed recv
                FAILED, 
                UNKNOWN_DEST, // could not find dest ident
                PROPOGATION_NODE,
                PAPER_MSG_GENERATED, // not just paper msg, but like, anything where generation is the last step and we can't confirm recpt.

            };

            enum SENDER {
                UNKNOWN = 0,
                ME = 1,
                THEM = 2
            };
            Message();
            Message(RNS::Bytes msg); // over the wire already packed
            Message(RNS::Bytes src,RNS::Bytes dest, string title, string content); // made by an app, will need to pack
            Message(JsonArray array); // serialized in json/msgpack. Already packed and unpacked.  TODO: DO we need to double up like this? maybe we only load the unpacked versions to save RAM
            RNS::Bytes src;
            RNS::Bytes dest;
            RNS::Bytes signature;
            RNS::Bytes packed_payload;

            string title;
            string content;
            time_t timestamp = 0;
            mutable SENDER sender = UNKNOWN;
            
            STATUS status = STATUS::UNSET;

            RNS::Bytes fullMsg() const;
            // helper for quickly figuring out who sent a message
            boolean msgSentByThem(RNS::Bytes& their_hash) const {
                switch(sender) {
                    case ME:
                       return false;
                    case THEM:
                        return true;
                    default:
                        if(src == their_hash) {
                            sender = THEM;
                            return true;
                        } 
                        sender = ME;
                        return false;
                }
            }
            void pack(RNS::Destination& src, RNS::Destination& dest); // turn title/content/etc into packed buffer
            void unpack(); // turn packed buffer into title/content/etc
            void serialize(JsonArray &array);
    };

    class ConversationMetaInfo {
        public:
            const static uint32_t max_converstaions=100;
            RNS::Bytes their_hash;
            string their_name;
            time_t last_message_at;

            void clear();
            void serialize(JsonObject &obj);
            void deserialize(JsonObject &obj);

            boolean operator<(const ConversationMetaInfo& other) const {
                return this->last_message_at < other.last_message_at;
            }
            boolean operator>(const ConversationMetaInfo& other) const {
                return this->last_message_at > other.last_message_at;
            }
            boolean operator==(const ConversationMetaInfo& other) const {
                return last_message_at == other.last_message_at && their_hash == other.their_hash;
            }
    };

    class Conversation {
        protected:
            std::list<Message> msgs; // use the accessors please :)
        public:
            static const uint32_t max_messages = 200;
            ConversationMetaInfo info;
        
            void addMessage(const Message &msg);
            const std::list<Message>& getMessages() const;

            void clear();
            void serialize(JsonDocument &doc); 
            void deserialize(JsonDocument &doc);

    };

    
    const set<AnnounceData>* getAnnounceData();
    void addAnnounceData(AnnounceData &a);
    void persistAnnounceData();

    set<ConversationMetaInfo>* getAllConversationInfo();
    void persistAllConversationInfo();

    // only one conversation at a time
    Conversation* loadAsCurrentConversation(const RNS::Bytes &src_hash);
    void persistCurrentConversation();
    // their_hash is the other party in the conversation (sender for received, recipient for sent)
    void addMessageToConversation(const Message &msg, const RNS::Bytes &their_hash);
}





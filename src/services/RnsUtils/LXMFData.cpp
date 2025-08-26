#include "LXMFData.h"
#include "retOS/retOS.h"

using namespace Retcon::LXMF;

static set<AnnounceData> lxmf_data;
static boolean loaded = false;

#define LXMF_ANNOUNCE_DATA_FILE_PATH "/lxmf/lxmf_announce_data.bin"
#define LXMF_CONVERSATION_FOLDER "/lxmf/conversations/"


AnnounceData::AnnounceData(JsonArray &array) {
    MsgPackBinary destb = array[0].as<MsgPackBinary>();
    dest.assign((uint8_t*)destb.data(), destb.size());

    MsgPackBinary adb = array[1].as<MsgPackBinary>();
    app_data.assign((uint8_t*)adb.data(), adb.size());

    last_heard = array[2];
}

AnnounceData::AnnounceData(const RNS::Bytes &dest, const RNS::Bytes &app_data, const time_t last_heard) : dest(dest), app_data(app_data), last_heard(last_heard){

}

void AnnounceData::serialize(JsonArray &array)  {
    array.clear();
    array.add(MsgPackBinary(dest.data(), dest.size()));
    array.add(MsgPackBinary(app_data.data(), app_data.size()));
    array.add(last_heard);
}

Retcon::LXMF::Message::Message() 
{
}


Retcon::LXMF::Message::Message(RNS::Bytes msg) :
    dest(msg.mid(0, 16)), src(msg.mid(16, 16)),
    signature(msg.mid(2*16, 64)), packed_payload(msg.mid(2*16 + 64))
{
}

Retcon::LXMF::Message::Message(RNS::Bytes src, RNS::Bytes dest, string title, string content) :src(src), dest(dest), title(title), content(content) {

}

Retcon::LXMF::Message::Message(JsonArray array) {
    // only unpkaced bits are stored
    MsgPackBinary srcb = array[0].as<MsgPackBinary>();
    dest.assign((uint8_t*)srcb.data(), srcb.size());

    MsgPackBinary destb = array[1].as<MsgPackBinary>();
    dest.assign((uint8_t*)destb.data(), destb.size());

    title = array[2].as<string>();
    content = array[3].as<string>();
    status = (STATUS) array[4].as<uint8_t>();
} 

void Retcon::LXMF::Message::serialize(JsonArray &array) {
    // only unpkaced bits are stored
    array.add(MsgPackBinary(src.data(), src.size()));
    array.add(MsgPackBinary(dest.data(), dest.size()));
    array.add(title);
    array.add(content);
    array.add((uint8_t) status);

} 

 void Retcon::LXMF::Message::pack(RNS::Destination& src, RNS::Destination& dest) {
    // fill timestamp with NOW()
    time(&timestamp);

    JsonDocument reply_payload;
    reply_payload.add(timestamp);
    reply_payload.add(MsgPackBinary(title.c_str(), title.size()));
    reply_payload.add(MsgPackBinary(content.c_str(), content.size()));
    reply_payload.add(nullptr); //fields. dont support em for now
    

    uint8_t packed_payload[max_lxmf_payload_size];
    size_t bytes_written = serializeMsgPack(reply_payload, packed_payload, max_lxmf_payload_size);

    this->dest = dest.hash();
    this->src = src.hash();

    RNS::Bytes hashed_part;
    hashed_part.append(dest.hash());
    hashed_part.append(src.hash());
    hashed_part.append(packed_payload, bytes_written); 

    RNS::Bytes hash = RNS::Identity::full_hash(hashed_part);
    RNS::Bytes signed_part;
    signed_part.append(hashed_part);
    signed_part.append(hash);

    signature = src.sign(signed_part);
    this->packed_payload.assign(packed_payload, bytes_written);
    // RNS::Bytes packed;
    // packed.append(dest.hash());
    // packed.append(src.hash());
    // packed.append(reply_sig);
    // packed.append(packed_reply_payload, bytes_written);
    //RNS::Packet *send_packet = new RNS::Packet(srcDest, packed);
 }
void Retcon::LXMF::Message::unpack() {
    JsonDocument msg;
    deserializeMsgPack(msg, packed_payload.data(), packed_payload.size());
    // print to serial for debug
    //serializeJsonPretty(msg, Serial);

    timestamp = msg[0];

    MsgPackBinary titlebin = msg[1];
    MsgPackBinary contentsbin = msg[2];

    title = string((const char*)titlebin.data(), titlebin.size());
    content = string((const char*)contentsbin.data(), contentsbin.size());
}

RNS::Bytes Retcon::LXMF::Message::fullMsg() const { 
    return dest + src + signature + packed_payload;
}

set<AnnounceData>* Retcon::LXMF::getAnnounceData() {
    if(!loaded)  {
        FS* fs = retOsGlobalPtr->hal().fs;
        if(fs->exists(LXMF_ANNOUNCE_DATA_FILE_PATH)) {
            JsonDocument doc; 
            File dataFile = fs->open(LXMF_ANNOUNCE_DATA_FILE_PATH);
            DeserializationError error = deserializeMsgPack(doc, dataFile);
            // TODO version check
            if(error == DeserializationError::Code::Ok) {
                JsonArray data = doc["messages"];
                for(int i=0; i< data.size() && i< NUM_ANNOUNCES; i++) {
                    JsonArray msgArray = data[i].as<JsonArray>();
                    lxmf_data.insert(AnnounceData(msgArray));
                }
            }
                
            doc.clear();
            dataFile.close();
        }
        loaded = true;
    }
    
    return &lxmf_data;
}

void Retcon::LXMF::persistAnnounceData(){
    // trim to size
    while(lxmf_data.size() > NUM_ANNOUNCES) {
        lxmf_data.erase(std::prev(lxmf_data.end()));
    }

     FS* fs = retOsGlobalPtr->hal().fs;
     JsonDocument doc;
     doc["version"] = LXMF_SCHEMA_VERSION;
     doc["messages"].createNestedArray();

     JsonArray msgs = doc["messages"];
     // copy the announce data into the struct
     uint32_t i = 0;
     for(AnnounceData ad: lxmf_data) {
        JsonArray msgArray = msgs[i++].createNestedArray();
        ad.serialize(msgArray);
     }

     File dataFile = fs->open(LXMF_ANNOUNCE_DATA_FILE_PATH,"w", true);
     serializeMsgPack(doc, dataFile);
     dataFile.close();
     doc.clear();
}

static Conversation current_conv;
// temp conversation for quickly loading then deleting
// TODO: Should we use a queue or something? in theory someone could have 2 full conversations open here
// TODO: in addition to all the buffer overhead for serialization and file writing/reading O.o
static Conversation temp_conv;
// Set of ALL conversation metainfo so we can easily show a list or whatever
static set<ConversationMetaInfo> conversations_set;

static void load_converstion(const RNS::Bytes &src_hash, Conversation &conv) {
    string hexId = src_hash.toHex();
    FS* fs = retOsGlobalPtr->hal().fs;

    string path = LXMF_CONVERSATION_FOLDER + hexId + ".bin";
    conv.clear();

    if(fs->exists(path.data())) {
        JsonDocument doc;
        
        File f = fs->open(path.data(), "r");
        deserializeMsgPack(doc, f);
        f.close();

        conv.deserialize(doc);
        doc.clear();
    } else {
        conv.info.their_hash = src_hash;
    }

}

void persistConversation(Conversation &conv) {
    if(conv.info.their_hash.size() > 0) {
        string hexId = conv.info.their_hash.toHex();
        FS* fs = retOsGlobalPtr->hal().fs;

        string path = LXMF_CONVERSATION_FOLDER + hexId + ".bin";
        JsonDocument doc;
        conv.serialize(doc);
        
        File f = fs->open(path.data(), "w", true);
        serializeMsgPack(doc, f);
        f.close();
        doc.clear();
    }
}

Conversation* Retcon::LXMF::loadAsCurrentConversation(const RNS::Bytes &src_hash) {
    getAllConversationInfo();
    load_converstion(src_hash, current_conv);
    // make sure current conv is in the list
    conversations_set.insert(current_conv.info);
    return &current_conv;
}

void Retcon::LXMF::persistCurrentConversation() {
    persistConversation(current_conv);
}

void Retcon::LXMF::addMessageToConversation(const Message &msg) {

    if(msg.src == current_conv.info.their_hash) {
        current_conv.addMessage(msg);
    } else {
        // ouch -- gotta load the whole thing to persist a single new message
        load_converstion(msg.src, temp_conv);
        temp_conv.addMessage(msg);
        persistConversation(temp_conv);
        // make sure it's in the meta list
        conversations_set.insert(temp_conv.info);
        temp_conv.clear();
    }   
}

void Conversation::addMessage(const Message &msg) {
    if(msgs.size() > max_messages) {
        msgs.pop_front();
    }
    msgs.push_back(msg);
}

const std::list<Message>& Conversation::getMessages() const {
    return msgs;
}

void Conversation::clear() {
    info.clear();
    msgs.clear();
}

void ConversationMetaInfo::clear() {
    their_hash.clear();
    their_name.clear();
    last_message_at = 0;
}

void ConversationMetaInfo::serialize(JsonObject &obj) {
    obj["their_hash"] = MsgPackBinary(their_hash.data(), their_hash.size());
    obj["their_name"] = their_name;

}

void ConversationMetaInfo::deserialize(JsonObject &obj) {
    MsgPackBinary thb = obj["their_hash"].as<MsgPackBinary>();
    their_hash.assign((uint8_t*)thb.data(), thb.size());

    their_name = obj["their_name"].as<string>();

}

void Conversation::serialize(JsonDocument &doc) {
    JsonObject infoObj = doc["info"].createNestedObject();
    info.serialize(infoObj);
    JsonArray msgListArray = doc["messages"].createNestedArray();

    int i = 0;
    for(Message msg : this->msgs) {
       JsonArray msgArray =  msgListArray[i++].createNestedArray();
       msg.serialize(msgArray);
    }

}

void Conversation::deserialize(JsonDocument &doc) {
    JsonObject infoObj = doc["info"];
    info.deserialize(infoObj);

    JsonArray msgListArray = doc["messages"];
    for(int i = 0; i < msgListArray.size() && i < max_messages; i++) {
        JsonArray msgArray = msgListArray[i];
        msgs.push_back(Message(msgArray));
    }
}



set<ConversationMetaInfo>* Retcon::LXMF::getAllConversationInfo() {
    if(conversations_set.size() > 0) return &conversations_set;

    FS* fs = retOsGlobalPtr->hal().fs;
    string path = LXMF_CONVERSATION_FOLDER  "message_set.bin";

    if(fs->exists(path.data())) {
        JsonDocument doc;
        
        File f = fs->open(path.data(), "r");
        deserializeMsgPack(doc, f);
        f.close();

        for(int i=0; i<doc.size();i++){
            JsonObject obj = doc[i];
            ConversationMetaInfo info;
            info.deserialize(obj);
            conversations_set.insert(info);
        }

        doc.clear();
    }

    return &conversations_set;
}

void Retcon::LXMF::persistAllConversationInfo(){
    if(conversations_set.size() > 0) {
        FS* fs = retOsGlobalPtr->hal().fs;
        string path = LXMF_CONVERSATION_FOLDER  "message_set.bin";

        JsonDocument doc;
        int i = 0;
        for(ConversationMetaInfo info: conversations_set) {
            if(i < ConversationMetaInfo::max_converstaions) {
                JsonObject obj = doc[i++].createNestedObject();
                info.serialize(obj);
            }
        }
        File f = fs->open(path.data(), "w", true);
        serializeMsgPack(doc, f);
        f.close();
        doc.clear();
    }
}
/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include <vector>
#include <unordered_map>

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5

#define NUM_NBRS 4

#define SIZEOF_HEADER sizeof(MessageHdr)
#define SIZEOF_HBENTRY sizeof(MemberListEntry)
#define SIZEOF_ADDR sizeof(Address::addr)

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
    MEMB_TABLE,
    DUMMYLASTMSGTYPE
};

/**
 * DESCRIPTION: Header of the message
 * Data members are arranged in order larger to smaller in size, so there is no hole in between
 */
struct MessageHdr {
	int numEntries;
	char msgType;
};

/**
 * DESCRIPTION: Heartbeat Entry in the message.
 * Data members are arranged in order larger to smaller in size, so there is no hole in between
 */
struct HeartbeatEntry {
	long heartbeat;
	int id;
	short port;
};

Address makeAddress(int id, short port);

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];
	//unordered_map<long long, int> addr2IdxHT; 

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();

	void DbgLog(Address *addr, const char* msg);
	char* prepMsgForJoinReq(int& msgLen);
	char* prepMsgForSharingMemberTable(enum MsgTypes msgType, int& msgLen);
	int getMembTableEntry(Address addr);
	void addMembTableEntry(HeartbeatEntry* entry);
	void updateMembTableEntry(HeartbeatEntry* entry);

	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);
	void nodeLoopOps();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void initMemberListTable(Member *memberNode);
	void printAddress(Address *addr);
	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */

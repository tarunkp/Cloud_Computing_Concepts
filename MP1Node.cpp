/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"


/**
 * Function to make address from id and port.
 * The class Address does not provide this basic functionality, so this function is needed.
 * Object of MemberListEntry stores address in form of id and port.
 * 
 */
Address makeAddress(int id, short port) {
	Address address;
	*(int *)(&address.addr[0]) = id;
	*(short *)(&address.addr[4]) = port;
	return address;
}

void getIdPortInfo(Address& address, int& id, short& port) {
	id = *(int *)(&address.addr[0]);
	port = *(short *)(&address.addr[4]);
}

/*
Address myAddress(Member *node) {
	makeAddress(node->addr.addr
}
*/

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/** 
 * These ifdef' are ugly in the code, so adding a wrapper over the log function.
 */
void MP1Node::DbgLog(Address *addr, const char* msg) {
#ifdef DEBUGLOG
	log->LOG(addr, msg);
#endif
}

int MP1Node::getMembTableEntry(Address addr) {
	/*
	long long key = 0;
	*(Address*)key = addr;
	if(addr2IdxHT.count(key)) {
		return addr2IdxHT[key];
	}
	*/
	return -1;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
	DbgLog(&memberNode->addr, "init_thisnode failed. Exit.");
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
	DbgLog(&memberNode->addr, "Unable to join self to group. Exiting.");
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	/* Unused variables
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);
	*/

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

bool makeHeartbeatMsgEntryInPlace(HeartbeatEntry* entry, Address& addr, long heartbeat) {
	int id; short port;
	getIdPortInfo(addr, id, port);
	entry->id = id;
	entry->port = port;
	entry->heartbeat = heartbeat;
	//memcpy((char *)entry, &addr.addr, SIZEOF_HEADER);
	//memcpy((char *)(entry + SIZEOF_HEADER), &heartbeat, SIZEOF_HBENTRY);
	return true;
}

bool makeHeartbeatMsgEntryInPlace(HeartbeatEntry* entry, MemberListEntry tblEntry) {
	entry->id = tblEntry.id;
	entry->port = tblEntry.port;
	entry->heartbeat = tblEntry.heartbeat;
	return true;
}

/**
 * This function returns C string containing message for join request.
 * Caller has responsibility to free memory.
 */
char* MP1Node::prepMsgForJoinReq(int& msgLen) {
	msgLen = SIZEOF_HEADER + SIZEOF_HBENTRY;
	char *msg = (char *) malloc(msgLen);
	MessageHdr *header = (MessageHdr *)msg;

	header->msgType = JOINREQ;
	header->numEntries = 1;
	char *ptr = msg + SIZEOF_HEADER;

	HeartbeatEntry* entry = (HeartbeatEntry *)(ptr);
	makeHeartbeatMsgEntryInPlace(entry, memberNode->addr, memberNode->heartbeat);
	return msg;
}

char* MP1Node::prepMsgForSharingMemberTable(enum MsgTypes msgType, int& msgLen) {
	int tableSize = memberNode->memberList.size();
	msgLen = SIZEOF_HEADER + (tableSize +1) * SIZEOF_HBENTRY;
	char *msg = (char *) malloc(msgLen);
	MessageHdr *header = (MessageHdr *)msg;
	header->msgType = msgType;
	header->numEntries = tableSize +1;

	char *ptr = msg + SIZEOF_HEADER;
	HeartbeatEntry* entry = (HeartbeatEntry *)(ptr);
	makeHeartbeatMsgEntryInPlace(entry, memberNode->addr, memberNode->heartbeat);
	ptr += SIZEOF_HBENTRY;
	for(auto& tblEntry : memberNode->memberList) {
		HeartbeatEntry* entry = (HeartbeatEntry *)(ptr);
		if(par->getcurrtime() > tblEntry.timestamp + TFAIL) {
			header->numEntries -= 1;
			continue;
		}
		makeHeartbeatMsgEntryInPlace(entry, tblEntry);
		ptr += SIZEOF_HBENTRY;
	}

	msgLen = ptr - msg;
	return msg;
}


/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
    if (memberNode->addr == *joinaddr) {
        // I am the group booter (first process to join the group). Boot up the group
	DbgLog(&memberNode->addr, "Starting up group...");
        memberNode->inGroup = true;
    }
    else {
	DbgLog(&memberNode->addr, "Trying to join...");
        // send JOINREQ message to introducer member
	int msgLen = 0;
	char* msg = prepMsgForJoinReq(msgLen);
        emulNet->ENsend(&memberNode->addr, joinaddr, msg, msgLen);
        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
	/*
	memberNode->bFailed = false;
	memberNode->inited = false;
	memberNode->inGroup = false;
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = 0;
	memberNode->timeOutCounter = -1;
	memberNode->memberList.clear();
	*/

	return 1;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

void MP1Node::addMembTableEntry(HeartbeatEntry* entry) {
	MemberListEntry newMember(entry->id, entry->port, entry->heartbeat, par->getcurrtime());
	memberNode->memberList.push_back(newMember);

	auto neighbour = makeAddress(entry->id, entry->port);
	log->logNodeAdd(&(memberNode->addr), &neighbour);
}


void MP1Node::updateMembTableEntry(HeartbeatEntry* entry) {
	if(memberNode->addr == makeAddress(entry->id, entry->port))
		return;
	bool done = false;
	for(auto& tblEntry : memberNode->memberList) {
		if (tblEntry.id == entry->id && tblEntry.port == entry->port) {
			if(tblEntry.heartbeat < entry->heartbeat) {
				tblEntry.heartbeat = entry->heartbeat;
				tblEntry.timestamp = par->getcurrtime();
			}
			done = true;
			break;
		}
	}
	if(!done) {
		addMembTableEntry(entry);
	}
}


/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
	MessageHdr *header = (MessageHdr *)data;
	char msgType = header->msgType;
	int numEntries = header->numEntries;

	char *ptr = data + SIZEOF_HEADER;

	if (msgType == JOINREQ) {
		HeartbeatEntry* entry = (HeartbeatEntry *)(ptr);
		Address newNodeAddr = makeAddress(entry->id, entry->port);

		int msgLen = 0;
		char* msg = prepMsgForSharingMemberTable(JOINREP, msgLen);
        	emulNet->ENsend(&memberNode->addr, &newNodeAddr, msg, msgLen);
	        free(msg);

		addMembTableEntry(entry);

		return true;
	}

	for(int i=0; i < numEntries; ++i) {
		HeartbeatEntry* entry = (HeartbeatEntry *)(ptr);
		updateMembTableEntry(entry);
		ptr += SIZEOF_HBENTRY;
	}

	if (msgType == JOINREP) {
		memberNode->inGroup = true;
	}
	return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
	auto& membTable = memberNode->memberList;
	size_t tblSize = membTable.size(), i = 0;

	// Remove entry after T-cleanup (TREMOVE)
	while(i < tblSize) {
		if(par->getcurrtime() > membTable[i].timestamp + TREMOVE) {
			auto addr = makeAddress(membTable[i].id, membTable[i].port);
			log->logNodeRemove(&memberNode->addr, &addr);
			membTable.erase(membTable.begin() +i);
			--tblSize;
		} else {
			++i;
		}
	}

	// Increment node's heartbeat
	memberNode->heartbeat++;

	int msgLen = 0;
	char* msg = prepMsgForSharingMemberTable(MEMB_TABLE, msgLen);

	// Send member table to randomly selected neighbours
	for(i=0; i < NUM_NBRS && i < tblSize; ++i) {
		int idx = std::rand() % tblSize;
		auto neighbour = makeAddress(membTable[idx].id, membTable[idx].port);
        	emulNet->ENsend(&memberNode->addr, &neighbour, msg, msgLen);

	}

	free(msg);

	return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

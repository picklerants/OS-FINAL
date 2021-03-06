/** This program illustrates the server end of the message queue **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <vector>
#include <list>
#include <string>
#include <fstream>
#include <iostream>
#include "msg.h"
#include <time.h>
using namespace std;

struct record
{

    /* The record id */
    int id;

    /* The first name */
    string firstName;

    /* The first name */
    string lastName;

    /**
     * Prints the record
     * @param fp - the stream to print to
     */
    void print(FILE* fp)
    {
        fprintf(stderr, "%d--%s--%s", id, firstName.c_str(), lastName.c_str());
    }
};



/**
 * The structure of a hashtable cell
 */
class hashTableCell
{
    /* Public members */
public:
    pthread_mutex_t sMutex;


    /**
     * Initialize the mutex
     */
    hashTableCell()
    {
        /* TODO: Initialize the mutex associated with this cell */
        sMutex = PTHREAD_MUTEX_INITIALIZER;
    }

    /**
     * Initialize the mutex
     */
    ~hashTableCell()
    {
        if(pthread_mutex_destroy(&sMutex)!=0){
            perror("deallocate mutex");
            exit (-1);
        }
    }

    /**
     * Locks the cell mutex
     */
    void lockCell()
    {
        if(pthread_mutex_lock(&sMutex)!=0){
            perror("lock");
            exit (-1);
        }
    }

    /**
     * Unlocks the cell mutex
     */
    void unlockCell()
    {
        if(pthread_mutex_unlock(&sMutex)!=0){
            perror("unlock");
            exit (-1);
        }
    }


    /* The linked list of records */
    list<record> recordList;


    /**
     * TODO: Declare the cell mutex
     */

};

/* The number of cells in the hash table */
#define NUMBER_OF_HASH_CELLS 100

/* The number of inserter threads */
#define NUM_INSERTERS 5

/* The hash table */
vector<hashTableCell> hashTable(NUMBER_OF_HASH_CELLS);

/* The number of fetcher threads */
vector<pthread_t> fetcherThreadIds;

/* The thread ids */
vector<pthread_t> threadsIds(NUM_INSERTERS);

/* The number of threads */
int numThreads = 0;

/* The message queue id */
int msqid;

/* A flag indicating that it's time to exit */
bool timeToExit = false;

/* The number of threads that have exited */
int numExitedThreads = 0;

/* The ids that yet to be looked up */
list<int> idsToLookUpList;

/* The mutex protecting idsToLookUpList */
pthread_mutex_t idsToLookUpListMutex = PTHREAD_MUTEX_INITIALIZER;

void ctrlCSignal(int signal)
{

    cout << "pressed ctrl c\n\n\n";

    /* Free system V resources */
    exit(0);
}

/**
 * Prototype for createInserterThreads
 */
void createInserterThreads();


/**
 * A prototype for adding new records.
 */
void* addNewRecords(void* arg);

void* addNewRecords2(void* arg){
    while(true){
        signal(SIGINT, ctrlCSignal);
        addNewRecords(NULL);
    }
}


/**
 * Deallocated the message queue
 * @param sig - the signal
 */
void cleanUp(int sig)
{

    /* Finally, deallocate the queue */
    if(msgctl(msqid, IPC_RMID, NULL) < 0)
    {
        perror("msgctl");
    }

    /* It's time to exit */
    timeToExit = true;
}

/**
 * Sends the message over the message queue
 * @param msqid - the message queue id
 * @param rec - the record to send
 */
void sendRecord(const int& msqid, const record& rec)
{
    /**
     * Convert the record to message
     */

    /* The message to send */
    message msg;

    /* Copy fields from the record into the message queue */
    msg.messageType = SERVER_TO_CLIENT_MSG;
    msg.id = rec.id;
    strncpy(msg.firstName, rec.firstName.c_str(), MAX_NAME_LEN);
    strncpy(msg.lastName, rec.lastName.c_str(), MAX_NAME_LEN);

    /* Send the message */
    sendMessage(msqid, msg);
}

/**
 * Prints the hash table
 */
void printHashTable()
{
    /* Go through the hash table */
    for(vector<hashTableCell>::const_iterator hashIt = hashTable.begin();
        hashIt != hashTable.end(); ++hashIt)
    {

        /* Go through the list at each hash location */
        for(list<record>::const_iterator lIt = hashIt->recordList.begin();
            lIt != hashIt->recordList.end(); ++lIt)
        {
            fprintf(stderr, "%d-%s-%s-%d\n", lIt->id,
                    lIt->firstName.c_str(),
                    lIt->lastName.c_str(),
                    lIt->id % NUMBER_OF_HASH_CELLS
                    );
        }
    }
}

/**
 * Adds a record to hashtable
 * @param rec - the record to add
 */
void addToHashTable(const record& rec)
{
    /**
     * TODO: Lock the mutex associated with the cell
     */

    int modNum = rec.id%NUMBER_OF_HASH_CELLS;

    hashTable.at(modNum).lockCell();


    /* TODO: Add the record to the cell */

    hashTable.at(modNum).recordList.push_back(rec);


    /**
     * TODO: release mutex associated with the cell
     */

    hashTable.at(modNum).unlockCell();


}


/**
 * Adds a record to hashtable
 * @param id the id of the record to retrieve
 * @return - the record from hashtable if exists;
 * otherwise returns a record with id field set to -1
 */
record getHashTableRecord(const int& id)
{
    /* The data structure to hold the record */
    record rec;

    /* TODO: Find the cell containing the record with the specificied ID. Search
     * the linklist of records in that cell and see if its ID matches. If so, return that
     * record. If not, return a record with ID of -1 indicating that no such record is present.
     * PLEASE REMEMBER: YOU NEED TO AVOID RACE CONDITIONS ON THE RECORDS IN THE SAME CELL.
     * You will need to lock the appropriate cell mutex.
     */

    int mod = id%NUMBER_OF_HASH_CELLS;

    hashTable.at(mod).lockCell();

    list<record>::const_iterator lIt = hashTable.at(mod).recordList.begin();

    while(lIt!=hashTable.at(mod).recordList.end()){
        if(lIt->id==id){
            rec.firstName = lIt->firstName;
            rec.id = lIt->id;
            rec.lastName = lIt->lastName;
            hashTable.at(mod).unlockCell();
            return rec;
        }
        ++lIt;
    }

    //cant find record return -1
    rec.id = -1;
    hashTable.at(mod).unlockCell();
    return rec;
}


/**
 * Loads the database into the hashtable
 * @param fileName - the file name
 * @return - the number of records left.
 */
int populateHashTable(const string& fileName)
{	
    /* The record */
    record rec;

    /* Open the file */
    ifstream dbFile(fileName.c_str());

    /* Is the file open */
    if(!dbFile.is_open())
    {
        fprintf(stderr, "Could not open file %s\n", fileName.c_str());
        exit(-1);
    }


    /* Read the entire file */
    while(!dbFile.eof())
    {
        /* Read the id */
        dbFile >> rec.id;

        /* Make sure we did not hit the EOF */
        if(!dbFile.eof())
        {
            /* Read the first name and last name */
            dbFile >> rec.firstName >> rec.lastName;

            /* Add to hash table */
            addToHashTable(rec);
        }
    }

    /* Close the file */
    dbFile.close();
}

/**
 * Gets ids to process from work list
 * @return - the id of record to look up, or
 * -1 if there is no work
 */
int getIdsToLookUp()
{
    /* The id of the looked up record */
    int id = -1;

    /* TODO: Check if idsToLookUpList (see global declaration) contains any
     * requests (i.e., ids of requested records).  If so, then remove that ID and
     * return it. Otherwise, return -1 indicating that the queue is empty. Please
     * remember that multiple threads can access this queue. Please make sure that
     * there are no race conditions. You can use the globally declared mutex
     * idsToLookUpListMutex. If you use a different mutex, please make sure you
     * use the same mutex in addIdsToLookUp (or face race conditions).
     */


    if (pthread_mutex_lock(&idsToLookUpListMutex) != 0){
        perror("lock");
        exit(-1);
    }


    if (!idsToLookUpList.empty()){
        id = idsToLookUpList.front();
        idsToLookUpList.pop_front();
    }

    if (pthread_mutex_unlock(&idsToLookUpListMutex) != 0){
        perror("unlock");
        exit(-1);
    }


    return id;
}

/**
 * Add id of record to look up
 * @param id - the id to process
 */
void addIdsToLookUp(const int& id)
{
    /* TODO: Add the id of the requested record to the idsToLookUpList (globally declared).
     * Please make sure there are no race conditions. Protect all accesses using the approach
     * described in getIdsToLookUp().
     */
    //check if locked using idsToLookUpListMutex
    if (pthread_mutex_lock(&idsToLookUpListMutex) !=0){
        perror("lock");
        exit(-1);
    }


    idsToLookUpList.push_back(id);

    if (pthread_mutex_unlock(&idsToLookUpListMutex) !=0){
        perror("unlock");
        exit(-1);
    }


}

/**
 * The thread function where the thread
 * services requests
 * @param thread argument
 */
void* fetcherThreadFunction(void* arg)
{
    /* TODO: The thread should stay in a loop until the user presses
     * Ctrl-c.  The thread should check if there are any requests on the
     * idsToLookUpList (you can use getIdsToLookUp() and if there are look up the
     * record in the hashtable (you can use getHashTableRecord()) and send it back
     * to the client (you can use sendRecord())
     */


    record tempRec;
    int tempID;

    while (true){
        tempID = getIdsToLookUp();
        signal(SIGINT, ctrlCSignal);

        if (tempID == -1){
        }

        else {
            tempRec = getHashTableRecord(tempID);
            cout << tempRec.id << "\t" << tempRec.firstName <<"\t" <<  tempRec.lastName << endl;
            //            sendRecord(msqid, tempRec);
        }
    }

}

/**
 * Creates threads that update the database
 * with randomly generated records
 */
void createInserterThreads()
{

    /* TODO: Create NUM_INSERTERS number of threads that
     * will be constantly inserting records to the hash table.
     * Threads can use addNewRecords() for that.
     */

    for (int i=0; i< NUM_INSERTERS; i++){

        if(pthread_create(&threadsIds.at(i), NULL, addNewRecords2, NULL) != 0)
        {
            perror("pthread_create");
            exit(-1);
        }
    }

}

/**
 * Creates the specified number of fetcher threads
 * @param numFetchers - the number of fetcher threads
 */
void createFetcherThreads(const int& numFetchers)
{
    /* TODO: Create numFetchers threads that call fetecherThreadFunction().
     * These threads will be responsible for responding to client requests
     * with records looked up in the hashtable.
     */

    //    for (int i=0; i< numFetchers; i++){
    //        pthread_create()
    //    }
}

/**
 * Called by parent thread to process incoming messages
 */
void processIncomingMessages()
{
    /* TODO: This function will be called by the parent thread.
     * It will wait to recieve requests from the client on the
     * message queue (you can use recvMessage() and message queue was
     * already created for you (msqid is the id; see main()).
     */
}

/**
 * Generates a random record
 * @return - a random record
 */
record generateRandomRecord()
{
    /* A record */
    record rec;

    /* Generate a random id */
    rec.id = rand() % NUMBER_OF_HASH_CELLS;

    /* Add the fake first name and last name */
    rec.firstName = "Random";
    rec.lastName = "Record";

    return rec;
}

/**
 * Threads inserting new records to the database
 * @param arg - some argument (unused)
 */
void* addNewRecords(void* arg)
{	
    /* TODO: This function will be called by the threads responsible for
     * performing random generation and insertion of new records into the
     * hashtable.
     */

    int num = 1000;
    int id = rand()%num + 1;

    record rec;
    rec.id = id;
    rec.firstName = "?";
    rec.lastName = "?";

    int mod = id%NUMBER_OF_HASH_CELLS;

    //lock cell
    hashTable.at(mod).lockCell();

    //get size of recordList
    int size = hashTable.at(mod).recordList.size();

    //init walker
    list<record>::const_iterator lIt = hashTable.at(mod).recordList.begin();

    //iterate through recordList


    while(lIt != hashTable.at(mod).recordList.end()){

        //if duplicate dont insert and then unlock
        if (lIt->id == id){
            hashTable.at(mod).unlockCell();
            return 0;
        }
        ++lIt;
    }


    hashTable.at(mod).recordList.push_back(rec);
    hashTable.at(mod).unlockCell();
}

/**
 * Tells all threads to exit and waits until they exit
 * @param numThreads - the number of threads that retrieve
 * records from the hash table.
 */
void waitForAllThreadsToExit()
{
    /* TODO: Wait for all inserter threads and fetcher threads to join */


    for (int i=0; i< NUM_INSERTERS; i++){

        if(pthread_join(threadsIds.at(i), NULL) != 0)
        {
            perror("pthread_create");
            exit(-1);
        }
    }

}





int main(int argc, char** argv)
{

    srand(time(NULL));

    /**
     * Check the command line
     */
    if(argc < 2)
    {
        fprintf(stderr, "USAGE: %s <DATABASE FILE NAME>\n", argv[0]);
        exit(-1);
    }

    /* Install a signal handler */
    if(signal(SIGINT, cleanUp) == SIG_ERR)
    {
        perror("signal");
        exit(-1);
    }

    /* Populate the hash table */
    populateHashTable(argv[1]);

    createInserterThreads();

    printHashTable();


    return 0;


    //testing add
    cout << "testing add 500\n\n\n";

//    for (int i=0; i< 500; i++){
//        int num = 1000;
//        int* ptr = &num;
//        addNewRecords(ptr);
//    }

//    printHashTable();

    cout << "testing retrieve\n\n\n\n\n";


    for (int i=0; i< 500; i++){
        int num = rand()%1000 +1;
        record rec1;
        rec1 = getHashTableRecord(num);
        if(rec1.id!=-1){
            cout << rec1.id << "\t" << rec1.firstName << "\t" << rec1.lastName << endl;
        }
        else {
            cout << "record not found\n";
        }
    }

//    for (int i=1; i<= 10; i++){
//        idsToLookUpList.push_back(i);
//    }

//    fetcherThreadFunction(NULL);

    /* Create the threads */
//    createFetcherThreads(numThreads);

    /* Create the inserter threads */
    createInserterThreads();

    printHashTable();


    return 0;


    /* Get the number of threads */
    numThreads = atoi(argv[2]);

    /* Use a random file and a random character to generate
     * a unique key. Same parameters to this function will
     * always generate the same value. This is how multiple
     * processes can connect to the same queue.
     */
    key_t key = ftok("/bin/ls", 'O');

    /* Connect to the message queue */
    msqid = createMessageQueue(key);


    /* Instantiate a message buffer */
    message msg;

    /* Create the threads */
    createFetcherThreads(numThreads);

    /* Create the inserter threads */
    createInserterThreads();

    /* Process incoming requests */
    processIncomingMessages();
    fprintf(stderr, "Here");

    /* Terminate all threads */
    waitForAllThreadsToExit();

    /* Free the mutex used for protecting the work queue */
    if(pthread_mutex_destroy(&idsToLookUpListMutex) != 0)
        perror("pthread_mutex_destroy");


    return 0;
}

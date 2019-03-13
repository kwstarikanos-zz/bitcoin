#include <malloc.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include "wallet.h"
#include "transaction.h"
#include "bitcoin.h"

/* Execute
 * transaction*/
bool execute(Wallet senderWallet, listPtr senderTransactions, Wallet receiverWallet, listPtr receiverTransactions,
             Transaction transaction) {
    assert(senderWallet != NULL);
    assert(senderTransactions != NULL);
    assert(receiverWallet != NULL);
    assert(receiverTransactions != NULL);
    assert(transaction != NULL);
    assert(transaction->value > 0);

    unsigned long int rest = transaction->value;
    bitcoin bc = NULL;
    printf("\n• • • E X E C U T E   T R A N S A C T I O N • • •\n");

    /* Access each bitcoin of sender to perform transaction*/
    while (rest > 0 && (bc = listNext(senderWallet->bitcoins)) != NULL) {
        printf("Try with bitcoin [%lu] ", bcGetId(bc));
        bcInsert(bc, &rest, transaction);
        printf("rest: [%lu]\n", rest);
    }

    if (rest > 0) {
        // return false;
    }

    printf("\n• • • • • • • • • • • • • • • • • • • • • • • • •\n");
    return true;
}

/* Perform
 * transaction from input buffer*/
bool performTransaction(char *token, hashtable *wallets, hashtable *senderHashtable, hashtable *receiverHashtable,
                        hashtable *transactionsHashtable) {
    bool error = false;
    char *line = NULL, *transactionId = NULL;
    Transaction transaction = NULL;
    Wallet senderWallet = NULL, receiverWallet = NULL;
    listPtr senderTransactions = NULL, receiverTransactions = NULL;

    /* Allocate space for line string to save a copy of token.*/
    line = malloc(strlen(token) * sizeof(char) + 1);
    if (line != NULL) {
        strcpy(line, token);
        transactionId = strtok(token, " ");
        printf("Transaction: [%s]\n", line);

        /*Create a transaction through hashtable from parsed line to ensure there is no other one with the same id.*/
        if (HT_Insert(*transactionsHashtable, transactionId, line, (void **) &transaction)) {
            assert(transaction != NULL);

            /* Get sender's wallet.*/
            senderWallet = HT_Get(*wallets, transaction->senderWalletId);
            assert(senderWallet != NULL);

            /* Get receiver's wallet.*/
            receiverWallet = HT_Get(*wallets, transaction->receiverWalletId);
            assert(receiverWallet != NULL);

            /* Create/Get sender transactions list hashtable*/
            assert(HT_Insert(*senderHashtable, transaction->senderWalletId, transaction->senderWalletId,
                             (void **) &senderTransactions));

            assert(senderTransactions != NULL);

            /*Create/Get receiver transactions list hashtable*/
            assert(HT_Insert(*receiverHashtable, transaction->senderWalletId, transaction->senderWalletId,
                             (void **) &receiverTransactions));

            assert(receiverTransactions != NULL);

            if (transaction->timestamp < max_transaction_timestamp) {
                error = true;
                fprintf(stdout, "\nTransaction date is less than the current time!\n");
            }

            if (!error) {
                if (transaction->value > senderWallet->balance) {
                    error = true;
                    fprintf(stdout, "\nThe sender's money is not enough to complete the transaction!\n");
                } else {
                    if (execute(senderWallet, senderTransactions, receiverWallet, receiverTransactions,
                                transaction)) {
                        if (transaction->timestamp > max_transaction_timestamp) {
                            max_transaction_timestamp = transaction->timestamp;
                        }
                    } else {
                        fprintf(stdout, "\nTransaction fail!\n");
                        error = true;
                    }
                }
            }
        } else {
            fprintf(stdout, "\nTransaction '%s' is duplicate!\n", transaction->transactionId);
            error = true;
        }
        printf("\n");
        free(line);
    } else {
        error = true;
        fprintf(stdout, "\nUnexpected error!\n");
    }
    return error;
}

/* Perform
 * transactions from input stream*/
bool performTransactions(FILE *fp, hashtable *walletsHT, hashtable *senderHT,
                         hashtable *receiverHT, hashtable *transactionsHT, char *delimiter) {
    bool error = false;
    char buf[BUF], *token = NULL;
    while (fgets(buf, BUF, fp) != NULL) {
        token = strtok(buf, delimiter);
        if (token != NULL) {
            error = performTransaction(token, walletsHT, senderHT, receiverHT, transactionsHT);
        } else {
            break;
        }
    }
    return error;
}

/* @Callback
 * Initialize & return a new transaction*/
Transaction createTransaction(char *token) {
    Transaction transaction = NULL;
    struct tm t;
    int x = 0;

    /*Initialize tm to avoid valgrind errors*/
    t.tm_mday = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_mday = 0;
    t.tm_mon = 0;
    t.tm_year = 0;
    t.tm_wday = 0;
    t.tm_yday = 0;
    t.tm_isdst = 0;

    transaction = (Transaction) malloc(sizeof(struct Transaction));
    if (transaction != NULL) {

        token = strtok(token, " "); // TransactionId
        transaction->transactionId = malloc(strlen(token) * sizeof(char *) + 1);
        if (transaction->transactionId) {
            strcpy(transaction->transactionId, token);
        }

        token = strtok(NULL, " "); // SenderWalletId
        transaction->senderWalletId = malloc(strlen(token) * sizeof(char *) + 1);
        if (transaction->senderWalletId != NULL) {
            strcpy(transaction->senderWalletId, token);
        }

        token = strtok(NULL, " "); // RecieverWalletId
        transaction->receiverWalletId = malloc(strlen(token) * sizeof(char *) + 1);
        if (transaction->receiverWalletId != NULL) {
            strcpy(transaction->receiverWalletId, token);
        }

        token = strtok(NULL, " "); // Amount
        transaction->value = (unsigned long int) strtol(token, NULL, 10);

        token = strtok(NULL, ""); // Date time

        /*Parse date & time*/
        x = sscanf(token, "%d-%d-%d %d:%d", &t.tm_mday, &t.tm_mon, &t.tm_year, &t.tm_hour, &t.tm_min);

        if (x == 5) {
            transaction->timestamp = mktime(&t);
            if (transaction->timestamp < 0) {
                fprintf(stdout, "\nBad datetime!\n");
                return NULL;
            }
        } else {
            fprintf(stdout, "\nBad datetime format!\n");
            return NULL;
        }
    }
    return transaction;
}

/* @Callback
 * Compare transaction with userId field*/
int cmpTransaction(Transaction transaction, char *transactionId) {
    return strcmp(transaction->transactionId, transactionId);
}

/* @Callback
 * Hash function for transactions hashtable*/
unsigned long int transactionHash(char *key, unsigned long int capacity) {
    int i, sum = 0;
    size_t keyLength = strlen(key);
    for (i = 0; i < keyLength; i++) {
        sum += key[i];
    }
    return sum % capacity;
}

/* @Callback
 * Destroy transaction*/
void destroyTransaction(Transaction transaction) {
    free(transaction->transactionId);
    free(transaction->senderWalletId);
    free(transaction->receiverWalletId);
    free(transaction);
}

/* @Callback
 * Initialize & return a new transaction list*/
listPtr createTransactionList(char *userId) {
    listPtr list = NULL;
    listCreate(&list, userId);
    return list;
}

/* @Callback
 * Compare keys function for transaction lists hashtable*/
int cmpTransactionList(listPtr tr1, char *userId) {
    return listGetIdentifier(tr1) != userId;
}

/* @Callback
 * Destroy function for transaction hashtable*/
void destroyTransactionList(listPtr list) {
    listDestroy(&list);
}

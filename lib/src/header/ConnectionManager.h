#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H



class ConnectionManager {
    public:
        void configure(unsigned int listenPort);
    private:

        int socketfd, newsocketfd;
        char buffer[];
};



#endif //CONNECTIONMANAGER_H

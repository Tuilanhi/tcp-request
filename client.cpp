#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>
#include <utility>
#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

void patient_thread_function(/* add necessary arguments */ int p_no, int n, BoundedBuffer *request_buffer)
{
    // functionality of the patient threads

    // take a patient p_no
    // for n requests, produce a datamsg(p_no, time, ECGNO) and push to BoundedBuffer request_buffer
    //      - time dependent on current requests
    //      - at 0 -> time = 0.00; at 1 -> time = 0.004; at 2 -> time = 0.008
    double time = 0;
    for (int i = 0; i < n; i++)
    {
        datamsg msg(p_no, time, EGCNO);
        char c[sizeof(datamsg)];
        memcpy(c, &msg, sizeof(datamsg));
        request_buffer->push(c, sizeof(c));
        time += 0.004;
    }
}

void file_thread_function(/* add necessary arguments */ int m, string file_name, BoundedBuffer *request_buffer, TCPRequestChannel *new_chan)
{
    // functionality of the file thread

    // file size
    int64_t offset = 0;
    filemsg fm(offset, 0);

    int buffer_capacity = sizeof(filemsg) + file_name.size() + 1;

    char *buf = new char[buffer_capacity];
    memcpy(buf, &fm, sizeof(filemsg));
    // append the file_name to the buffer that contains the file message
    strcpy(buf + sizeof(filemsg), file_name.c_str());

    new_chan->cwrite(buf, buffer_capacity);
    int64_t filesize;
    new_chan->cread(&filesize, sizeof(int64_t));

    delete[] buf;

    // open output file; allocate the memory fseek; close the file
    FILE *fp;
    fp = fopen(file_name.c_str(), "wb");
    fseek(fp, filesize, SEEK_SET);
    fclose(fp);

    // while offset < file_size, produce a filemsg(offset, m)+filename and push to request_buffer
    //      - incrementing offset; and be careful with the final message
    while (offset < filesize)
    {
        if (filesize - offset > m)
        {
            // cannot append the filemessage with file_name
            // create a buffer to store filemessage and file_name
            filemsg f(offset, m);
            // create pointer to buffer of the size of filemsg + filename
            char *buf2 = new char[buffer_capacity];
            memcpy(buf2, &f, sizeof(filemsg));
            // append the file_name to the buffer that contains the file message
            strcpy(buf2 + sizeof(filemsg), file_name.c_str());

            request_buffer->push(buf2, buffer_capacity);
            delete[] buf2;
        }
        else
        {
            int last_chunk = filesize - offset;
            // cannot append the filemessage with file_name
            // create a buffer to store filemessage and file_name
            filemsg f(offset, last_chunk);
            // create pointer to buffer of the size of filemsg + filename
            char *buf2 = new char[buffer_capacity];
            memcpy(buf2, &f, sizeof(filemsg));
            // append the file_name to the buffer that contains the file message
            strcpy(buf2 + sizeof(filemsg), file_name.c_str());

            request_buffer->push(buf2, buffer_capacity);
            delete[] buf2;
        }
        offset += m;
    }
}

void worker_thread_function(int m, string file_name, BoundedBuffer *request_buffer, BoundedBuffer *response_buffer, TCPRequestChannel *chan)
{
    // functionality of the worker threads

    // forever loop
    // pop message from the request_buffer
    // view line 120 in server.cpp (process_request()) for how to decide current message
    // send the message across a FIFO channel
    // collect response
    // if DATA:
    //      - create a pair of p_no from message and response from server
    //      - push that pair to the response_buffer
    // if FILE:
    //      - collect the filename from the message
    //      - open the file in update mode
    //      - fseek(SEEK_SET) to offset of the filemsg
    //      - write the buffer from the server
    while (true)
    {
        char c[1024];
        request_buffer->pop(c, 1024);
        MESSAGE_TYPE _m = *((MESSAGE_TYPE *)c);
        if (_m == DATA_MSG)
        {
            chan->cwrite(&c, sizeof(datamsg));
            double reply;
            chan->cread(&reply, sizeof(double));

            datamsg *response = (datamsg *)c;
            pair<int, double> p;
            p.first = response->person;
            p.second = reply;
            response_buffer->push((char *)(&p), sizeof(p));
        }
        else if (_m == FILE_MSG)
        {
            string file_path;
            if (file_name != "")
            {
                file_path = string("received/") + file_name;
            }
            filemsg fm = *((filemsg *)c);
            int buffer_capacity = sizeof(filemsg) + file_name.size() + 1;

            FILE *fp;
            fp = fopen(file_path.c_str(), "wb");
            fseek(fp, fm.offset, SEEK_SET);

            chan->cwrite(c, buffer_capacity);
            char *buf = new char[m];
            chan->cread(buf, fm.length);
            fwrite(buf, sizeof(char), fm.length, fp);

            delete[] buf;
        }
        else if (_m == QUIT_MSG)
        {
            chan->cwrite(&_m, sizeof(MESSAGE_TYPE));
            break;
        }
    }
}

void histogram_thread_function(/* add necessary arguments */ HistogramCollection *hc, BoundedBuffer *response_buffer)
{
    // functionality of the histogram threads

    // forever loop
    // special package equivalent to QUIT_MSG to stop the loop
    // pop response from the response_buffer
    // call HC:update(response->p_no, response->double)

    // set the size of char * to the size of pair<int,double> which is 12 ( 4 + 8)
    // convert char * to pair<int,double> pointer
    // pair<int,double> pointer.first is the int p_no we push into the response_buffer from worker_thread(), and pointer.second is the double response from the server
    while (true)
    {
        char x[sizeof(pair<int, double>)];
        response_buffer->pop(x, sizeof(x));
        pair<int, double> *p = (pair<int, double> *)x;
        if (p->first == -1 && p->second == -1)
        {
            break;
        }
        hc->update(p->first, p->second);
    }
}

int main(int argc, char *argv[])
{
    int n = 1000;        // default number of requests per "patient"
    int p = 10;          // number of patients [1,15]
    int w = 100;         // default number of worker threads
    int h = 20;          // default number of histogram threads
    int b = 20;          // default capacity of the request buffer (should be changed)
    int m = MAX_MESSAGE; // default capacity of the message buffer
    string f = "";       // name of file to be transferred
    bool is_file = false;
    string host = "";
    string port = "";

    // read arguments
    int opt;
    while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            n = atoi(optarg);
            break;
        case 'p':
            p = atoi(optarg);
            break;
        case 'w':
            w = atoi(optarg);
            break;
        case 'h':
            h = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 'm':
            m = atoi(optarg);
            break;
        case 'f':
            f = optarg;
            is_file = true;
            break;
        case 'a':
            host = optarg;
            break;
        case 'r':
            port = optarg;
            break;
        }
    }

    // initialize overhead (including the control channel)
    TCPRequestChannel *chan = new TCPRequestChannel(host, port);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
    HistogramCollection hc;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }

    // record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    // vector or array of patient_thread (if data, size with p element; if file, size is 1 element -> single file thread)

    // array of FIFOs (w element)

    // array of worker threads (w elements)

    // array of histogram threads (if data, h element; if file, 0 element (empty pointer))

    vector<thread> worker;
    vector<TCPRequestChannel *> channels;

    // create w worker_threads (store in worker array)
    //      -> create w channels (store in FIFO array)

    // create w channels before creating worker threads

    for (int i = 0; i < w; i++)
    {
        TCPRequestChannel *new_chan = new TCPRequestChannel(host, port);
        channels.push_back(new_chan);
    }

    for (int i = 0; i < w; i++)
    {
        worker.push_back(thread(worker_thread_function, m, f, &request_buffer, &response_buffer, channels[i]));
    }

    /*
    / DATAMSG REQUEST
    */
    if (is_file == false)
    {
        vector<thread> producer;
        vector<thread> histograms;

        // create p patient_threads
        for (int i = 1; i <= p; i++)
        {
            producer.push_back(thread(patient_thread_function, i, n, &request_buffer));
        }

        // // create h histogram_threads
        for (int i = 0; i < h; i++)
        {
            histograms.push_back(thread(histogram_thread_function, &hc, &response_buffer));
        }

        /* join all threads here */
        // iterate over all thread arrays, calling join
        //         - order is important, patient->worker->histogram
        // first is patient_thread
        for (int i = 0; i < p; i++)
        {
            producer[i].join();
        }

        // send QUIT_MSGs to worker_thread so it knows when to break out from the loop
        MESSAGE_TYPE quit = QUIT_MSG;
        for (int i = 0; i < w; i++)
        {
            char msg[sizeof(MESSAGE_TYPE)];
            memcpy(msg, &quit, sizeof(MESSAGE_TYPE));
            request_buffer.push(msg, sizeof(msg));
        }

        // second is the worker_thread
        for (int i = 0; i < w; i++)
        {
            worker[i].join();
        }

        for (int i = channels.size() - 1; i >= 0; i--)
        {
            delete channels[i];
        }

        // push -1 to histogram_thread(), loop through number of h times
        for (int i = 0; i < h; i++)
        {
            pair<int, double> p;
            p.first = -1;
            p.second = -1;
            response_buffer.push((char *)(&p), sizeof(p));
        }

        // third is the histogram threads
        for (int i = 0; i < h; i++)
        {
            histograms[i].join();
        }
    }

    /*
    / FILEMSG REQUEST
    */
    else
    {
        thread file_thread(file_thread_function, m, f, &request_buffer, chan);

        // JOINING ALL THREADS
        // patient->worker->histogram
        file_thread.join();

        // send QUIT_MSGs to worker_thread so it knows when to break out from the loop
        MESSAGE_TYPE quit = QUIT_MSG;
        for (int i = 0; i < w; i++)
        {
            char msg[sizeof(MESSAGE_TYPE)];
            memcpy(msg, &quit, sizeof(MESSAGE_TYPE));
            request_buffer.push(msg, sizeof(msg));
        }

        for (int i = 0; i < w; i++)
        {
            worker[i].join();
        }

        for (int i = channels.size() - 1; i >= 0; i--)
        {
            delete channels[i];
        }
    }

    // record end time
    gettimeofday(&end, 0);

    // print the results
    if (f == "")
    {
        hc.print();
    }
    int secs = ((1e6 * end.tv_sec - 1e6 * start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int)1e6);
    int usecs = (int)((1e6 * end.tv_sec - 1e6 * start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int)1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    // quit and close all channels in FIFO array
    // quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite((char *)&q, sizeof(MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;
}
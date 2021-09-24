# FileMQ implementation in C

## Why FileMQ?

"Request-reply is just a vulgar subclass of publish-subscribe" -- me, in "Software Architecture over 0MQ".

It's the API my 5-year old son can use. It runs on every machine there is. It carries binary objects of any size and format. It plugs into almost every application in existence. Yes, it's your file system.

So this is FileMQ. It's a "chunked, flow-controlled, restartable, cancellable, async, multicast file transfer ØMQ protocol", server and client. The protocol spec is at http://rfc.zeromq.org/spec:19. It's a rough cut, good enough to prove or disprove the concept.

What problems does FileMQ solve? Well, two main things. First, it creates a stupidly simple pub-sub messaging system that literally anyone, and any application can use. You know the 0MQ pub-sub model, where a publisher distributes files to a set of subscribers. The radio broadcast model, where you join at any time, get stuff, then leave when you're bored. That's FileMQ. Second, it wraps this up in a proper server and client that runs in the background. You may get something similar to the DropBox experience but there is no attempt, yet, at full synchronization, and certainly not in both directions.

## How to build and use

This code needs the freshest possible libsodium, libzmq, and CZMQ. To build:

    git clone git://github.com/jedisct1/libsodium.git
    cd libsodium
    ./autogen.sh
    ./configure && make check
    sudo make install
    sudo ldconfig
    cd ..

    git clone git://github.com/zeromq/libzmq.git
    cd libzmq
    ./autogen.sh
    ./configure && make check
    sudo make install
    sudo ldconfig
    cd ..

    git clone git://github.com/zeromq/czmq.git
    cd czmq
    ./autogen.sh
    ./configure && make check
    sudo make install
    sudo ldconfig
    cd ..

    git clone git://github.com/hintjens/filemq.git
    cd filemq
    ./autogen.sh
    ./configure && make check
    sudo make install
    sudo ldconfig
    cd ..

Contribution process:

    http://rfc.zeromq.org/spec:22

## Internals

I originally designed FileMQ as a training tool for Chapter 6 of the 0MQ Guide (soon in dead tree format from O'Reilly!) and sort of a reusable stack for your own experiments in protocol development. Many concepts in FileMQ are borrowed from other protocols, such as the original AMQP (which, you may not know, had a file transfer class before that was taken out to a dark alley and beaten to death by two red-hatted goons from a firm who shall remain nameless). Of course AMQP borrowed much from other protocols, like its SASL authentication dialog, which came from BEEP, and which I happily used in FILEMQ as well.

SASL is the simple authentication and security layer, and a neat way to make secure protocols over 0MQ. It works for the specific case where peer A talks to peer B over a bidirectional connection. That is any classic client-server model, which is what FILEMQ (the protocol, in upper case), gives us.

Incidentally, it's client-to-server, not peer-to-peer. Fully-symmetric protocols are nasty. Far easier to define client and server as "roles" and allow any node to be either, or both. Which is what the filemq main application is.

If you look at that program (src/filemq.c) you'll see it creates a server object, a client object, runs them until someone interrupts the process, and then quits. Nice and simple.

The whole thing is written in C, but using my special sauce of class-orientation and generic containers that makes it almost as nice as Python to write, and to read. I'm sure Erlang still outclasses this, but for sheer coverage, it's hard to beat perfect C.

So there's a client class, and a server class, and then a bunch of utility classes, some of which are more or less reusable for other applications. There's a neat configuration file parser and the SASL class (which is still empty but will hopefully grow). The directory and file management classes may be a little specific to FileMQ's use case.

The real magic sauce in FileMQ is in the model directory, which you can safely ignore until you feel confident enough to go take a look. As a first start, check out the zeromq/zproto project, and fmq_msg.xml. You will need to grab gsl from https://github.com/zeromq/gsl if you want to generate the code. Check the generate script to see how we run gsl.

So that's code generation. You can, if you absolutely hate the gsl tool (and people do), you can get the same results with 10x more work using XSTL. There are other ways; in some languages you can parse the XML at runtime. That is, BTW, a rather stupid idea since it means anyone running the code needs the protocol spec at hand.

Now, take a look at fmq_client.xml and fmq_server.xml, and their matching compilers in client_c.gsl and server_c.gsl. These churn out quite ordinary multi-threaded clients and servers for any given state machine. It should take you a week or so to understand what the heck is actually going on. I'm not going to explain too much, it'll spoil the fun.

Let me know how you get on. I'll expand this README as you ask questions.

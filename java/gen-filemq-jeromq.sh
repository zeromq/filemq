#!/bin/bash

echo "Generate FileMQ codes use JeroMQ"

TARGET="jeromq"
JZMQ_VERSION="1.1.0-SNAPSHOT"
JEROMQ_VERSION="0.2.0"

if [ ! -d $TARGET ]; then
    mkdir $TARGET
fi

if [ ! -d $TARGET/src/test/resources ]; then
    mkdir -p $TARGET/src/test/resources
fi

cp -f src/test/resources/* $TARGET/src/test/resources

sed -e '
s/<artifactId>filemq<\/artifactId>/<artifactId>filemq-jeromq<\/artifactId>/
s/<name>filemq<\/name>/<name>filemq-jeromq<\/name>/
/<dependency>/,/<\/depencency>/ {
    /<groupId>org.zeromq<\/groupId>/,/<version>${JZMQ_VERSION}<\/version>/ { 
        s/zeromq/jeromq/
        s/jzmq/jeromq/
        s/'"${JZMQ_VERSION}"'/'"${JEROMQ_VERSION}"'/
    }
}' pom.xml > $TARGET/pom.xml

for j in `find ./src -name "*.java"`; do
    BASE=${j:2}
    FILE=${j##.*/}
    BASE=$TARGET/${BASE%/$FILE}
    mkdir -p $BASE
    sed -e '
    s/org.zeromq.ZMQ/org.jeromq.ZMQ/g
    s/org.zeromq.ZContext/org.jeromq.ZContext/g
    s/org.zeromq.ZFrame/org.jeromq.ZFrame/g
    s/org.zeromq.ZMsg/org.jeromq.ZMsg/g
    s/org.zeromq.ZThread/org.jeromq.ZThread/g
    ' $j > $BASE/$FILE
done

echo "Done"

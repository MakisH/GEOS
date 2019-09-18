#!bin/bash

SRC_PATH=$1
#SRC_PATH=`dirname $0`/../src
#echo $SRC_PATH

# Generate an updated schema
bin/geosx -s ${SRC_PATH}/coreComponents/fileIO/schema/schema.xsd

# Build the documentation tables
cd ${SRC_PATH}/coreComponents/fileIO/schema/
python SchemaToRSTDocumentation.py

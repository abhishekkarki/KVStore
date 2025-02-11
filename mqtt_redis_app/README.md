#  docker
$ to see the containers `docker ps`

$ to stop container `docker stop container_name _or_id`

$ to build an image `docker build -t <image_name>`

$ to get into the container `docker exec -it <containerid> bash`

# docker-compose
$to run docker-compose `docker-compose up`

$ to run this in the background `docker-compose up -d` -d is detached mode

$ to remove all the containers `docker-compose down`

$ to stop `docker-compose down`

$ remove all containers and unused images, network and build cache `docker system prune -af`

# Building the image
After updating the code run the following steps:
`docker compose up --build`

For stopping the containers 
`docker compose down`

If only app is changed then
`docker compose restart app`

# Redis
for opening a redis CLI attaching to the redis container
`docker exec -it <redis_container_name> redis-cli`

to open a interactive shell inside the container
`docker exec -it my_app_container /bin/bash`
if bash is not available `docker exec -it my_app_container /bin/sh`

Deleting specific time series key
`DEL temperature_esp`

Removing all the keys form the redis
`FLUSHALL`

To check the entries under the key temperature_esp
`TS.RANGE temperature_esp - +`

---------------------------------

Running all the containers
`docker compose up -d`

for running any particular container
`docker compose up -d redis` for redis
`docker compose up -d mosquitto` for mosquitto
`docker compose up -d grafana` for grafana
`docker compose up -d app` for the python app


to test locally by sending a mqtt message with the topic esp32/temperature
`mosquitto_pub -h localhost -p 1883 -t esp32/temperature -m '{"timestamp": 1730652955, "temperature": 25.0}'`



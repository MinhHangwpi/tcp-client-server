# tcp-client-server

## To compile

`server.c`
```
make
```
`client.c`
```
make
```

## To run
### To run `server.c`
```
./QRserver --PORT 8080 --RATE_MSGS 2 --RATE_TIME 60 --MAX_USERS 2 --TIME_OUT 60

or 

./QRserver -p 8080 -r 2 -t 60 -u 2 -o 60
```
*Note*: Please inspect the `parse_options()` function in the `server.c` file for further information on the options.
You will need to install java in your environment.

### To run `client.c`

```
./QRclient 8080
```
*Note*: Please edit the hardcoded IP address in the `client.c` file as needed.


Dependencies:
+ libcrypto++-dev
+ [sgx_common](https://github.com/rafaelppires/sgx_common)

To compile for SGX, run:
```
$ make SGX=1
```

To compile with debug symbols, run:
```
$ make DEBUG=1 [SGX=1]
```

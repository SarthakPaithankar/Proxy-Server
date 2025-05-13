# Proxy-Server

This is a web proxy server that supports HTTP 1.0/1.1 requests. A Web Proxy is a middle man that sits between your browser and the internet. The proxy can be used to cache static pages for fast page retrievel. It can also be used to block outgoing requests to certains website. You may see this implemented in corprate or professional setting where a organization may block users from accessing certain websites. A reg block list file can be added to the server to effectively block specified website request from completing.


Example Workflow:
A request is sent:

IF FILE IS CACHED:
  1.The Web Proxy will return the cached page
  
IF FILE IS NOT CACHED:
  1.The Web Proxy will forward the request to the internet.
  2.The Proxy will cache the new request
  3.The Proxy will serve the Client

Cached Request 
WEB BROWSER --> WEB PROXY XXX INTERNET
                    |
                    |
                    V
                  CACHE 
                  
WEB BROWSER <-- WEB PROXY XXX INTERNET
                    ^
                    |
                    |
                  CACHE 

                  
Uncached Request 
WEB BROWSER --> WEB PROXY --> INTERNET
                    |
                    |
                    V
                  CACHE 
                  
WEB BROWSER XXX WEB PROXY <-- INTERNET
                    |
                    |
                    V
                  CACHE 
                  
WEB BROWSER <-- WEB PROXY XXX INTERNET
                    ^
                    |
                    |
                  CACHE 
                  

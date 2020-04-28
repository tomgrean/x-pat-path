# libxpatpath
Small XML parser library with EXPAT backend and simple XPATH support


parameter xpath follows partial xml xpath rules. examples:
```
/web-app/session-config
/web-app/mime-mapping[2]
/web-app/servlet[servlet-name='jsp']/servlet-class
/Server/Service/Connector[@port='8009']
```
NOTE: the array index start from 1.


' #include "%s" U8 *req=FileRead("%s"); VC_CGI(,MemCpy(MSize(req)+1,req,MSize(req))); Exit(0);'

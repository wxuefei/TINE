F64 Fib(F64 n) {
	if(n<2)
		return n;
	return Fib(n-1)+Fib(n-2);
}
"%n\n",Fib(8);

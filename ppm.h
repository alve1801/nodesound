struct sprite{
  // yes i know olcpge has a sprite class, do i look like i know how to use it xD
  int x,y; // sizes
  short*data;
};

sprite loadimg(const char*filename){
	sprite ret={0,0,0};char a;
	FILE*f=fopen(filename,"r");
	if(!f){printf("Could not open <%s>\n",filename);exit(1);}
	for(int i=0;i<3;i++)getc(f); // P6\n
	while((a=getc(f))!=' ')ret.y=ret.y*10+a-48; // <X> 
	while((a=getc(f))!=10)ret.x=ret.x*10+a-48; // <Y>\n
	printf("%i %i\n",ret.x,ret.y);
	ret.data=(short*)malloc(ret.x*ret.y*sizeof(short));
	if(!ret.data){printf("Failed to allocate canvas\n");exit(1);}
	while((a=getc(f))!=10); // 255\n

	for(int x=0;x<ret.x;x++)
	for(int y=0;y<ret.y;y++)
		ret.data[x*ret.y+y]=
			(getc(f)>>4)<<8|
			(getc(f)>>4)<<4|
			(getc(f)>>4)<<0;
	fclose(f);
	printf("\n");
	return ret;
}

void debug_sprite(sprite s){
	for(int x=0;x<s.x;x++){
		for(int y=0;y<s.y;y++){
			printf("%03x ",s.data[x*s.y+y]);
		}
		putchar(10);
	}
}

void saveimg(sprite s){ // we prolly wont need this
	FILE*f=fopen("save.ppm","w");
	fprintf(f,"P6\n%i %i\n255\n",s.x,s.y);
	for(int y=0;y<s.y;y++){
	for(int x=0;x<s.x;x++){
		putc((s.data[x*s.y+y]&0x0f00)>>4,f);
		putc((s.data[x*s.y+y]&0x00f0)>>0,f);
		putc((s.data[x*s.y+y]&0x000f)<<4,f);
	}}
	fclose(f);
}

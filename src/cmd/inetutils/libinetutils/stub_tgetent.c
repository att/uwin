/* Return an error saying we couldn't find any termcap database.  */
int
tgetent (buf, type)
     char *buf, *type;
{
  return -1;
}

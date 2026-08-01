java -jar ../build/include/antlr-4.13.1-complete.jar -o .antlr TOP.g4 
javac -cp .:../build/include/antlr-4.13.1-complete.jar .antlr/*.java

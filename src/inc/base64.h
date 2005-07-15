/* Base64 encoding and decoding.
 * by Galt Barber */

#ifndef BASE64_H

#define B64CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

char *base64Encode(char *input);
/* Use base64 to encode a string.  Returns one long encoded
 * string which need to be freeMem'd. Note: big-endian algorithm.
 * For some applications you may need to break the base64 output
 * of this function into lines no longer than 76 chars.
 */

char *base64Decode(char *input);
/* Use base64 to decode a string.  Return decoded
 * string which will be freeMem'd. Note: big-endian algorithm.
 * Call eraseWhiteSpace() and check for invalid input 
 * before passing in input if needed.  
 */
 
#endif /* BASE64_H */


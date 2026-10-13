/* Deliberately invalid standalone C fixture. Not linked into Digit. */
int compute_total(int subtotal, int tax)
{
    return subtotal + missing_amount;
}

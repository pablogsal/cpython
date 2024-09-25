import math
from math import sumprod, sin


def quartic_kernel():
    """
    Defines the quartic kernel's PDF, CDF, inverse CDF, and support.
    """

    def pdf(t):
        """
        Probability density function (PDF) of the quartic kernel.
        """
        if abs(t) > 1:
            return 0.0
        return (15.0 / 16.0) * (1.0 - t * t) ** 2

    def cdf(t):
        """
        Cumulative distribution function (CDF) of the quartic kernel.
        """
        if t <= -1:
            return 0.0
        elif t >= 1:
            return 1.0
        else:
            return (3.0 / 16.0) * t**5 - (5.0 / 8.0) * t**3 + (15.0 / 16.0) * t + 0.5

    def inverse_cdf(p, tol=1e-12, max_iter=100):
        """
        Inverse CDF (quantile function) using Brent's method.

        Parameters:
            p (float): The probability for which to compute the quantile (0 <= p <= 1).
            tol (float): The tolerance for convergence.
            max_iter (int): Maximum number of iterations.

        Returns:
            float: The quantile corresponding to the probability p.
        """
        if not 0.0 <= p <= 1.0:
            raise ValueError("Probability p must be in the interval [0, 1].")
        if p == 0.0:
            return -1.0
        elif p == 1.0:
            return 1.0

        def func(x):
            return cdf(x) - p

        # Initial bracketing interval
        estimate = _quartic_invcdf_estimate(p)
        a, b = estimate - 0.1, estimate + 0.1
        fa, fb = func(a), func(b)

        if fa * fb > 0:
            raise ValueError("The root is not bracketed in the interval [-1, 1].")

        # Initialize variables
        c, fc = a, fa
        d = e = b - a

        for iteration in range(max_iter):
            print(abs(b - a))
            if fb == 0.0 or abs(b - a) < tol:
                return b

            if fa != fc and fb != fc:
                # Inverse quadratic interpolation
                s = (
                    a * fb * fc / ((fa - fb) * (fa - fc))
                    + b * fa * fc / ((fb - fa) * (fb - fc))
                    + c * fa * fb / ((fc - fa) * (fc - fb))
                )
            else:
                # Secant method
                s = b - fb * (b - a) / (fb - fa)

            # Conditions to check whether bisection is needed
            conditions = [
                not ((3 * a + b) / 4 < s < b if b > a else b < s < (3 * a + b) / 4),
                e < tol,
                abs(s - b) >= abs(e / 2),
                abs(e) >= tol,
                abs(fa) > abs(fb),
            ]

            if any(conditions):
                # Bisection method
                s = (a + b) / 2
                e = d = b - a

            fs = func(s)
            c, fc = b, fb  # Update c to previous b
            if fa * fs < 0:
                b, fb = s, fs
            else:
                a, fa = s, fs

            # Ensure that f(a) has a smaller absolute value
            if abs(fa) < abs(fb):
                a, b = b, a
                fa, fb = fb, fa

            # Update variables for next iteration
            d, e = e, b - a

        raise RuntimeError("Maximum number of iterations exceeded in inverse_cdf")

    support = 1.0
    return pdf, cdf, inverse_cdf, support


def _newton_raphson(f_inv_estimate, f, f_prime, tolerance=1e-12):
    def f_inv(y):
        "Return x such that f(x) ≈ y within the specified tolerance."
        x = f_inv_estimate(y)
        while abs(diff := f(x) - y) > tolerance:
            print(x)
            x -= diff / f_prime(x)
        return x

    return f_inv


def _quartic_invcdf_estimate(p):
    x = p - 0.5
    (16 / 15) * x - 3.5 * (8192 / 10125) * x**3 + (24117248 / 1139625) * x**5
    sign, p = (1.0, p) if p <= 1 / 2 else (-1.0, 1.0 - p)
    x = (2.0 * p) ** 0.4258865685331 - 1.0
    if p >= 0.004 < 0.499:
        x += 0.026818732 * sin(7.101753784 * p + 2.73230839482953)
    print(final, x)
    return x * sign


def quartic_kernel2():
    pdf = lambda t: 15 / 16 * (1.0 - t * t) ** 2
    cdf = lambda t: sumprod((3 / 16, -5 / 8, 15 / 16, 1 / 2), (t**5, t**3, t, 1.0))
    invcdf = _newton_raphson(_quartic_invcdf_estimate, f=cdf, f_prime=pdf)
    support = 1.0
    return pdf, cdf, invcdf, support


# Example usage
if __name__ == "__main__":
    pdf, cdf, invcdf, support = quartic_kernel()
    pdf2, cdf2, invcdf2, support2 = quartic_kernel2()

    import timeit
    import statistics

    # print(invcdf(0.01))
    print(invcdf2(0.01))

    # times1 = timeit.repeat(lambda: invcdf(0.01), number=10000, repeat=5)
    # times1 = statistics.mean(times1)
    # times2 = timeit.repeat(lambda: invcdf2(0.01), number=10000, repeat=5)
    # times2 = statistics.mean(times2)
    # print(times1, times2, times1 / times2 * 100)

    # Test the inverse CDF with a range of probabilities
    # probabilities = [i / 10 for i in range(11)]
    # print("p\tQuantile")
    # for p in probabilities:
    #     x = invcdf(p)
    #     x2 = invcdf2(p)
    #     print(f"{p:.2f}\t{x:.6f} - {p:.2f}\t{x2:.6f}")
    #
    # # Round-trip test: Compute CDF and then inverse CDF
    # x_values = [-0.8, -0.5, 0.0, 0.5, 0.8]
    # print("\nTesting inverse CDF (round-trip test):")
    # print("x\tCDF(x)\tInverseCDF(CDF(x))")
    # for x in x_values:
    #     p = cdf(x)
    #     x_recovered = invcdf(p)
    #     p2 = cdf2(x)
    #     x_recovered2 = invcdf2(p)
    #     print(
    #         f"{x:.1f}\t{p:.6f}\t{x_recovered:.6f} {x:.1f}\t{p2:.6f}\t{x_recovered2:.6f}"
    #     )

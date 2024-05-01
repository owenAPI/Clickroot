from selenium import webdriver
from selenium.webdriver.common.keys import Keys
import pandas as pd

driver = webdriver.Chrome('/Users/owen/Downloads/chromedriver_mac64/chromedriver')
driver.get('https://www.cboe.com/delayed_quotes/spy/quote_table')

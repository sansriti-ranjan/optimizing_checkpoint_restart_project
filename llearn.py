from __future__ import print_function, unicode_literals, division
import argparse
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torchvision import datasets, transforms
from torch.optim.lr_scheduler import StepLR
import os
from pathlib import Path
import cv2 as cv
import numpy as np


class MLP(nn.Module):
    def __init__(self, name):
        self.name = name
        

def main():

    #Prepare the training dataset


if __name__ == '__main__':
    main()